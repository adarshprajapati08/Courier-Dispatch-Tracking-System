(function () {
  'use strict';
  let volumeChart;
  let serviceChart;

  function makeStat(label, value, tone) {
    return '<article class="stat-card ' + tone + '"><div class="stat-label">' + UI.escapeHtml(label) + '</div><div class="stat-value">' + value + '</div><div class="stat-detail">Live SQLite report data</div></article>';
  }

  function renderFallback(target, items, labelKey, valueKey) {
    target.innerHTML = '<div class="legend-list">' + items.map(item => '<div class="legend-item"><span class="legend-label">' + UI.escapeHtml(item[labelKey]) + '</span><strong class="legend-value">' + item[valueKey] + '</strong></div>').join('') + '</div>';
  }

  function charts(data) {
    const monthly = data.monthly_statistics || [];
    const services = data.delivery_types || [];
    const volumeCanvas = document.querySelector('#monthly-chart');
    const serviceCanvas = document.querySelector('#service-chart');
    const volumeFallback = document.querySelector('#monthly-fallback');
    const serviceFallback = document.querySelector('#service-fallback');
    if (!window.Chart) {
      if (volumeCanvas) volumeCanvas.classList.add('hide');
      if (serviceCanvas) serviceCanvas.classList.add('hide');
      renderFallback(volumeFallback, monthly, 'month', 'total');
      renderFallback(serviceFallback, services, 'delivery_type', 'count');
      return;
    }
    if (volumeChart) volumeChart.destroy();
    if (serviceChart) serviceChart.destroy();
    if (volumeFallback) volumeFallback.innerHTML = '';
    if (serviceFallback) serviceFallback.innerHTML = '';
    volumeChart = new Chart(volumeCanvas, { type: 'bar', data: { labels: monthly.map(item => item.month), datasets: [{ label: 'Bookings', data: monthly.map(item => item.total), backgroundColor: '#2563eb', borderRadius: 6 }, { label: 'Delivered', data: monthly.map(item => item.delivered), backgroundColor: '#22a35a', borderRadius: 6 }] }, options: { responsive: true, maintainAspectRatio: false, plugins: { legend: { position: 'bottom', labels: { usePointStyle: true } } }, scales: { x: { grid: { display: false } }, y: { beginAtZero: true, ticks: { precision: 0 } } } } });
    serviceChart = new Chart(serviceCanvas, { type: 'doughnut', data: { labels: services.map(item => item.delivery_type), datasets: [{ data: services.map(item => item.count), backgroundColor: ['#2563eb', '#8b5cf6', '#19a35b', '#f59e0b'] }] }, options: { responsive: true, maintainAspectRatio: false, plugins: { legend: { position: 'bottom', labels: { usePointStyle: true } } } } });
  }

  async function init() {
    if (document.body.dataset.page !== 'reports') return;
    const stats = document.querySelector('#report-stats');
    try {
      const response = await API.get('/api/admin/reports');
      const summary = response.data.summary;
      stats.innerHTML = [makeStat('Total bookings', summary.total_bookings, 'blue'), makeStat('Successful deliveries', summary.successful_deliveries, 'green'), makeStat('Pending deliveries', summary.pending_deliveries, 'orange'), makeStat('Cancelled deliveries', summary.cancelled_deliveries, 'red'), makeStat('Failed deliveries', summary.failed_deliveries, 'red')].join('');
      charts(response.data);
    } catch (error) {
      stats.innerHTML = UI.emptyState('Reports unavailable', error.message);
      UI.toast(error.message, 'error');
    }
  }
  document.addEventListener('app:ready', init);
}());
