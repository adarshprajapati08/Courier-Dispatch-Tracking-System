(function () {
  'use strict';

  function stat(label, value, detail, tone) {
    return '<article class="stat-card ' + (tone || '') + '"><div class="stat-label">' + UI.escapeHtml(label) + '</div><div class="stat-value">' + UI.escapeHtml(String(value)) + '</div><div class="stat-detail">' + UI.escapeHtml(detail || '') + '</div></article>';
  }

  function shipmentRows(items, admin) {
    if (!items || !items.length) return '<tr><td colspan="' + (admin ? 6 : 5) + '">' + UI.emptyState('No shipments found', 'New courier bookings will appear here.') + '</td></tr>';
    return items.map(s => '<tr><td><a class="table-link" href="shipment-details.html?id=' + s.id + '">' + UI.escapeHtml(s.tracking_number) + '</a><span class="table-sub">' + UI.formatDate(s.booking_date) + '</span></td><td><span class="table-main">' + UI.escapeHtml(s.receiver_name || '') + '</span><span class="table-sub">' + UI.escapeHtml(s.receiver_city || s.destination || '') + '</span></td><td>' + UI.statusBadge(s.current_status) + '</td><td>' + UI.escapeHtml(s.delivery_type || '—') + '</td>' + (admin ? '<td>' + UI.escapeHtml(s.customer_name || 'Walk-in / Admin') + '</td>' : '') + '<td><a class="small-action" href="shipment-details.html?id=' + s.id + '">View</a></td></tr>').join('');
  }

  function renderStatusLegend(overview) {
    if (!overview || !overview.length) return UI.emptyState('No shipment data yet', 'The live status overview will appear after bookings are created.');
    const colors = { BOOKED: '#7a8da8', PICKED_UP: '#d58a12', IN_TRANSIT: '#2563eb', OUT_FOR_DELIVERY: '#8b5cf6', DELIVERED: '#16a34a', DELIVERY_FAILED: '#dc4452', CANCELLED: '#dc4452' };
    return '<div class="legend-list">' + overview.map(item => '<div class="legend-item"><span class="legend-label"><i class="legend-dot" style="background:' + (colors[item.status] || '#64748b') + '"></i>' + UI.escapeHtml(UI.statusLabel(item.status)) + '</span><strong class="legend-value">' + item.count + '</strong></div>').join('') + '</div>';
  }

  async function adminDashboard() {
    const stats = document.querySelector('#dashboard-stats');
    const shipments = document.querySelector('#recent-shipments-body');
    const updates = document.querySelector('#recent-updates');
    const overview = document.querySelector('#status-overview');
    try {
      const response = await API.get('/api/admin/dashboard');
      const data = response.data;
      const s = data.statistics;
      stats.innerHTML = [
        stat('Total shipments', s.total_shipments, 'All bookings on record', 'blue'),
        stat('Pending action', s.pending_shipments, 'Booked, pickup, or delivery action', 'orange'),
        stat('In transit', s.in_transit_shipments, 'Moving through the network', 'blue'),
        stat('Delivered', s.delivered_shipments, s.delivery_success_rate.toFixed(1) + '% completion rate', 'green'),
        stat('Cancelled', s.cancelled_shipments, 'Cancelled before delivery', 'red'),
        stat('Customers', s.total_customers, 'Registered customer accounts', ''),
        stat('Active staff', s.total_staff, 'Available operations staff', ''),
        stat('Success rate', s.delivery_success_rate.toFixed(1) + '%', 'Delivered ÷ total bookings', 'green')
      ].join('');
      shipments.innerHTML = shipmentRows(data.recent_shipments, true);
      overview.innerHTML = renderStatusLegend(data.status_overview);
      updates.innerHTML = data.recent_tracking_updates && data.recent_tracking_updates.length
        ? '<div class="timeline">' + data.recent_tracking_updates.map(e => '<article class="timeline-event"><span class="timeline-dot"></span><div><div class="timeline-title"><strong>' + UI.escapeHtml(UI.statusLabel(e.status)) + '</strong><span class="timeline-time">' + UI.formatDate(e.created_at, true) + '</span></div><span class="timeline-location">' + UI.escapeHtml(e.tracking_number) + ' · ' + UI.escapeHtml(e.location) + '</span><p>' + UI.escapeHtml(e.remarks) + '</p></div></article>').join('') + '</div>'
        : UI.emptyState('No tracking updates', 'Updates will appear when a shipment status changes.');
    } catch (error) {
      UI.toast(error.message, 'error');
      stats.innerHTML = UI.emptyState('Dashboard unavailable', error.message);
    }
  }

  function customerStats(items, profile) {
    const total = profile.shipment_count || items.length;
    const delivered = profile.delivered_count || items.filter(s => s.current_status === 'DELIVERED').length;
    const active = items.filter(s => !['DELIVERED', 'CANCELLED'].includes(s.current_status)).length;
    const cancelled = items.filter(s => s.current_status === 'CANCELLED').length;
    return [stat('My shipments', total, 'All courier bookings', 'blue'), stat('Active shipments', active, 'Currently being processed', 'orange'), stat('Delivered', delivered, 'Successfully completed', 'green'), stat('Cancelled', cancelled, 'Cancelled bookings', 'red')].join('');
  }

  function populateProfile(profile) {
    const form = document.querySelector('#profile-form');
    if (!form) return;
    form.elements['name'].value = profile.name || '';
    form.elements['email'].value = profile.email || '';
    form.elements['phone'].value = profile.phone || '';
    form.addEventListener('submit', async event => {
      event.preventDefault();
      const submit = form.querySelector('button[type="submit"]');
      UI.setBusy(submit, true, 'Saving…');
      try {
        const response = await API.put('/api/customer/profile', Object.fromEntries(new FormData(form).entries()));
        UI.toast('Profile updated successfully.', 'success');
        window.currentUser.name = response.data.name;
      } catch (error) { UI.toast(error.message, 'error'); }
      finally { UI.setBusy(submit, false); }
    });
  }

  async function customerDashboard() {
    const stats = document.querySelector('#customer-stats');
    const shipmentsBody = document.querySelector('#customer-shipments-body');
    try {
      const [profileResponse, shipmentsResponse] = await Promise.all([
        API.get('/api/customer/profile'), API.get('/api/customer/shipments?limit=100')
      ]);
      const profile = profileResponse.data;
      const items = shipmentsResponse.data.items || [];
      stats.innerHTML = customerStats(items, profile);
      shipmentsBody.innerHTML = shipmentRows(items.slice(0, 8), false);
      populateProfile(profile);
    } catch (error) {
      UI.toast(error.message, 'error');
      stats.innerHTML = UI.emptyState('Could not load your dashboard', error.message);
    }
  }

  function staffRows(items) {
    return items.length ? items.map(s => '<tr><td><a class="table-link" href="shipment-details.html?id=' + s.id + '">' + UI.escapeHtml(s.tracking_number) + '</a></td><td><span class="table-main">' + UI.escapeHtml(s.receiver_name) + '</span><span class="table-sub">' + UI.escapeHtml(s.receiver_city || s.destination || '') + '</span></td><td>' + UI.statusBadge(s.current_status) + '</td><td>' + UI.formatDate(s.estimated_delivery_date) + '</td><td><a class="small-action" href="shipment-details.html?id=' + s.id + '">Update</a></td></tr>').join('') : '<tr><td colspan="5">' + UI.emptyState('No assigned shipments', 'Assignments will appear here when an administrator assigns you work.') + '</td></tr>';
  }

  async function loadStaffShipments(search) {
    const body = document.querySelector('#staff-shipments-body');
    if (!body) return;
    body.innerHTML = '<tr><td colspan="5">Loading assigned shipments…</td></tr>';
    const params = new URLSearchParams({ limit: '100' });
    if (search) params.set('search', search);
    try { body.innerHTML = staffRows((await API.get('/api/staff/shipments?' + params)).data.items || []); }
    catch (error) { body.innerHTML = '<tr><td colspan="5">' + UI.emptyState('Could not load assigned shipments', error.message) + '</td></tr>'; UI.toast(error.message, 'error'); }
  }

  async function staffDashboard() {
    const stats = document.querySelector('#staff-stats');
    try {
      const response = await API.get('/api/staff/dashboard');
      const s = response.data.statistics;
      stats.innerHTML = [
        stat('Assigned', s.assigned_shipments, 'All shipments assigned to you', 'blue'),
        stat('Action required', s.action_required, 'In progress or delivery retry', 'orange'),
        stat('In transit', s.in_transit_shipments, 'At a network stage', 'blue'),
        stat('Delivered', s.delivered_shipments, 'Completed delivery assignments', 'green')
      ].join('');
      await loadStaffShipments('');
      const filter = document.querySelector('#staff-shipment-filters');
      if (filter) filter.addEventListener('submit', event => { event.preventDefault(); loadStaffShipments(filter.elements['search'].value.trim()); });
    } catch (error) {
      UI.toast(error.message, 'error');
      stats.innerHTML = UI.emptyState('Dashboard unavailable', error.message);
    }
  }

  document.addEventListener('app:ready', () => {
    const page = document.body.dataset.page;
    if (page === 'admin-dashboard') adminDashboard();
    if (page === 'customer-dashboard') customerDashboard();
    if (page === 'staff-dashboard') staffDashboard();
  });
}());
