(function () {
  'use strict';

  function timeline(history) {
    if (!history || !history.length) return UI.emptyState('No tracking activity yet', 'Updates will appear once the courier is processed.');
    return '<div class="timeline">' + history.map(event => '<article class="timeline-event"><span class="timeline-dot"></span><div><div class="timeline-title"><strong>' + UI.escapeHtml(UI.statusLabel(event.status)) + '</strong><span class="timeline-time">' + UI.formatDate(event.created_at, true) + '</span></div><span class="timeline-location">' + UI.escapeHtml(event.location || 'Location pending') + '</span><p>' + UI.escapeHtml(event.remarks || '') + '</p><span class="timeline-by">Updated by ' + UI.escapeHtml(event.updated_by || 'Courier team') + '</span></div></article>').join('') + '</div>';
  }

  function showShipment(shipment) {
    const target = document.querySelector('#tracking-result');
    if (!target) return;
    target.innerHTML = '<section class="tracking-result-header"><div><small>Tracking number</small><h2>' + UI.escapeHtml(shipment.tracking_number) + '</h2></div>' + UI.statusBadge(shipment.current_status) + '</section><section class="tracking-overview"><div><span>Current status</span><strong>' + UI.escapeHtml(UI.statusLabel(shipment.current_status)) + '</strong></div><div><span>Booked on</span><strong>' + UI.formatDate(shipment.booking_date) + '</strong></div><div><span>Estimated delivery</span><strong>' + UI.formatDate(shipment.estimated_delivery_date) + '</strong></div><div><span>Delivery type</span><strong>' + UI.escapeHtml(shipment.delivery_type) + '</strong></div></section><div class="tracking-result-grid"><article class="card route-card"><h3>Shipment route</h3><div class="route-points"><div class="route-point"><i class="route-pin"></i><div><span>Origin</span><strong>' + UI.escapeHtml(shipment.sender_city + ', ' + shipment.sender_state) + '</strong><small>' + UI.escapeHtml(shipment.sender_name) + '</small></div></div><div class="route-point"><i class="route-pin"></i><div><span>Destination</span><strong>' + UI.escapeHtml(shipment.receiver_city + ', ' + shipment.receiver_state) + '</strong><small>' + UI.escapeHtml(shipment.receiver_name) + '</small></div></div></div><hr class="soft-line"><div class="detail-list"><div><dt>Package</dt><dd>' + UI.escapeHtml(shipment.package_type) + ' · ' + UI.escapeHtml(String(shipment.package_weight)) + ' kg</dd></div></div></article><article class="card tracking-timeline-card"><h3>Tracking history</h3>' + timeline(shipment.tracking_history) + '</article></div>';
  }

  async function load(tracking) {
    const result = document.querySelector('#tracking-result');
    if (!tracking) return;
    if (result) result.innerHTML = '<div class="loading-block"><span><span class="spinner"></span>Loading shipment updates…</span></div>';
    try {
      const response = await API.get('/api/tracking/' + encodeURIComponent(tracking.trim().toUpperCase()));
      showShipment(response.data);
    } catch (error) {
      if (result) result.innerHTML = UI.emptyState('Shipment not found', error.message);
      UI.toast(error.message, 'error');
    }
  }

  function init() {
    const form = document.querySelector('#tracking-form');
    const input = document.querySelector('#tracking-number');
    if (!form || !input) return;
    form.addEventListener('submit', (event) => {
      event.preventDefault();
      const tracking = input.value.trim();
      if (!tracking) { UI.toast('Enter a tracking number to continue.', 'error'); return; }
      const url = new URL(window.location.href);
      url.searchParams.set('tracking', tracking.toUpperCase());
      window.history.replaceState({}, '', url);
      load(tracking);
    });
    const prefilled = new URLSearchParams(window.location.search).get('tracking');
    if (prefilled) { input.value = prefilled.toUpperCase(); load(prefilled); }
  }

  document.addEventListener('DOMContentLoaded', init);
}());
