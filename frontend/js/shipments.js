(function () {
  'use strict';

  let shipmentPage = 1;
  let cachedStaff = null;
  let activeDetailUser = null;

  const nextStates = {
    BOOKED: ['PICKED_UP', 'CANCELLED'],
    PICKED_UP: ['IN_TRANSIT', 'CANCELLED'],
    IN_TRANSIT: ['OUT_FOR_DELIVERY', 'DELIVERY_FAILED'],
    OUT_FOR_DELIVERY: ['DELIVERED', 'DELIVERY_FAILED'],
    DELIVERY_FAILED: ['OUT_FOR_DELIVERY', 'CANCELLED'],
    DELIVERED: [],
    CANCELLED: []
  };

  const shipmentStatuses = ['BOOKED', 'PICKED_UP', 'IN_TRANSIT', 'OUT_FOR_DELIVERY', 'DELIVERY_FAILED', 'DELIVERED', 'CANCELLED'];

  const e = UI.escapeHtml;
  const val = value => e(value === null || value === undefined ? '' : value);

  function optionList(current, choices) {
    return choices.map(choice => '<option value="' + choice + '"' + (current === choice ? ' selected' : '') + '>' + e(choice) + '</option>').join('');
  }

  function textField(name, label, value, required, span) {
    return '<div class="form-field' + (span ? ' span-2' : '') + '"><label>' + e(label) + (required ? ' <span class="required">*</span>' : '') + '</label><input name="' + name + '" value="' + val(value) + '"' + (required ? ' required' : '') + '></div>';
  }

  function selectField(name, label, value, choices) {
    return '<div class="form-field"><label>' + e(label) + ' <span class="required">*</span></label><select name="' + name + '" required>' + optionList(value, choices) + '</select></div>';
  }

  function shipmentFormMarkup(shipment, allowCustomer) {
    const s = shipment || {};
    return '<div class="form-grid">' +
      (allowCustomer ? '<div class="form-field span-2"><label>Registered customer ID <small>(Optional; leave blank for an admin/walk-in booking)</small></label><input type="number" min="1" name="customer_id" value="' + val(s.customer_id) + '"></div>' : '') +
      '<div class="form-field span-2"><h3 class="mini-form-title">Sender details</h3></div>' +
      textField('sender_name', 'Sender name', s.sender_name, true) + textField('sender_phone', 'Sender phone', s.sender_phone, true) +
      textField('sender_email', 'Sender email', s.sender_email, true) + textField('sender_pincode', 'Sender PIN code', s.sender_pincode, true) +
      textField('sender_address', 'Sender address', s.sender_address, true, true) + textField('sender_city', 'Sender city', s.sender_city, true) + textField('sender_state', 'Sender state', s.sender_state, true) +
      '<div class="form-field span-2"><h3 class="mini-form-title">Receiver details</h3></div>' +
      textField('receiver_name', 'Receiver name', s.receiver_name, true) + textField('receiver_phone', 'Receiver phone', s.receiver_phone, true) +
      textField('receiver_email', 'Receiver email', s.receiver_email, true) + textField('receiver_pincode', 'Receiver PIN code', s.receiver_pincode, true) +
      textField('receiver_address', 'Receiver address', s.receiver_address, true, true) + textField('receiver_city', 'Receiver city', s.receiver_city, true) + textField('receiver_state', 'Receiver state', s.receiver_state, true) +
      '<div class="form-field span-2"><h3 class="mini-form-title">Package details</h3></div>' +
      '<div class="form-field span-2"><label>Package description <span class="required">*</span></label><textarea name="package_description" required>' + val(s.package_description) + '</textarea></div>' +
      '<div class="form-field"><label>Package weight (kg) <span class="required">*</span></label><input type="number" step="0.01" min="0.05" max="100" name="package_weight" value="' + val(s.package_weight) + '" required></div>' +
      selectField('package_type', 'Package type', s.package_type || 'Parcel', ['Document', 'Parcel', 'Fragile', 'Electronics', 'Other']) +
      selectField('delivery_type', 'Delivery type', s.delivery_type || 'Standard', ['Standard', 'Express']) +
      '<div class="form-field"><label>Declared value (₹)</label><input type="number" step="0.01" min="0" name="declared_value" value="' + val(s.declared_value || 0) + '"></div>' +
      '</div>';
  }

  function objectFromShipmentForm(form) {
    const data = Object.fromEntries(new FormData(form).entries());
    data.package_weight = Number(data.package_weight);
    data.declared_value = Number(data.declared_value || 0);
    if ('customer_id' in data) {
      data.customer_id = data.customer_id ? Number(data.customer_id) : 0;
    }
    return data;
  }

  function pagination(meta, load) {
    if (!meta) return '';
    return '<footer class="pagination"><span>Showing page ' + meta.page + ' of ' + Math.max(meta.total_pages, 1) + ' · ' + meta.total + ' shipment' + (meta.total === 1 ? '' : 's') + '</span><div class="pagination-controls"><button type="button" data-page="' + (meta.page - 1) + '"' + (meta.page <= 1 ? ' disabled' : '') + '>‹</button><button type="button" data-page="' + (meta.page + 1) + '"' + (meta.page >= meta.total_pages || !meta.total_pages ? ' disabled' : '') + '>›</button></div></footer>';
  }

  function shipmentTable(items) {
    if (!items.length) return '<div class="panel-body">' + UI.emptyState('No shipments match your filters', 'Try a different tracking number, status, or date range.') + '</div>';
    return '<div class="table-wrap"><table class="data-table"><thead><tr><th>Tracking ID</th><th>Customer / receiver</th><th>Destination</th><th>Booked</th><th>Delivery</th><th>Status</th><th>Assigned staff</th><th>Actions</th></tr></thead><tbody>' + items.map(s => {
      const cancel = ['BOOKED', 'PICKED_UP', 'DELIVERY_FAILED'].includes(s.current_status)
        ? '<button class="icon-action danger cancel-admin-shipment" title="Cancel shipment" type="button" data-id="' + s.id + '" data-tracking="' + e(s.tracking_number) + '">×</button>' : '';
      return '<tr><td><a class="table-link" href="shipment-details.html?id=' + s.id + '">' + e(s.tracking_number) + '</a></td><td><span class="table-main">' + e(s.customer_name || 'Walk-in / Admin') + '</span><span class="table-sub">To: ' + e(s.receiver_name) + '</span></td><td>' + e(s.receiver_city) + '<span class="table-sub">' + e(s.receiver_state) + '</span></td><td>' + UI.formatDate(s.booking_date) + '</td><td>' + e(s.delivery_type) + '</td><td>' + UI.statusBadge(s.current_status) + '</td><td>' + e(s.assigned_staff_name || 'Unassigned') + '</td><td><div class="table-actions"><a class="icon-action" title="View, edit, or update status" href="shipment-details.html?id=' + s.id + '">⌕</a><button class="icon-action assign-shipment" title="Assign staff" type="button" data-id="' + s.id + '" data-tracking="' + e(s.tracking_number) + '">⇄</button>' + cancel + '</div></td></tr>';
    }).join('') + '</tbody></table></div>';
  }

  async function getStaff() {
    if (cachedStaff) return cachedStaff;
    const response = await API.get('/api/admin/staff?limit=100');
    cachedStaff = (response.data.items || []).filter(staff => staff.account_status === 'ACTIVE');
    return cachedStaff;
  }

  function showModal(title, body, actionText, onSubmit) {
    const container = document.createElement('div');
    container.className = 'modal-backdrop';
    container.innerHTML = '<div class="modal" role="dialog" aria-modal="true" aria-label="' + e(title) + '"><form id="modal-form"><header class="modal-head"><h2>' + e(title) + '</h2><button class="modal-close" type="button" aria-label="Close">×</button></header><div class="modal-body">' + body + '</div><footer class="modal-foot"><button class="button button-outline modal-close" type="button">Cancel</button><button class="button" type="submit">' + e(actionText) + '</button></footer></form></div>';
    document.body.appendChild(container);
    const close = () => container.remove();
    container.querySelectorAll('.modal-close').forEach(button => button.addEventListener('click', close));
    container.addEventListener('click', event => { if (event.target === container) close(); });
    const form = container.querySelector('#modal-form');
    form.addEventListener('submit', async event => {
      event.preventDefault();
      const button = form.querySelector('button[type="submit"]');
      UI.setBusy(button, true, 'Saving…');
      try { await onSubmit(form); close(); }
      catch (error) { UI.toast(error.message, 'error'); }
      finally { UI.setBusy(button, false); }
    });
  }

  async function openAssignModal(id, tracking) {
    let staff;
    try { staff = await getStaff(); }
    catch (error) { UI.toast(error.message, 'error'); return; }
    if (!staff.length) { UI.toast('Create an active staff account before assigning shipments.', 'error'); return; }
    showModal('Assign ' + tracking, '<div class="form-field"><label>Delivery staff member</label><select name="staff_id" required><option value="">Select an active staff member</option>' + staff.map(person => '<option value="' + person.id + '">' + e(person.name) + ' · ' + e(person.email) + '</option>').join('') + '</select></div>', 'Assign shipment', async form => {
      await API.put('/api/admin/shipments/' + id + '/assign', { staff_id: Number(new FormData(form).get('staff_id')) });
      UI.toast('Staff member assigned successfully.', 'success');
      await loadShipmentTable();
      if (document.body.dataset.page === 'shipment-details') await loadDetail();
    });
  }

  function queryFromFilters() {
    const form = document.querySelector('#shipment-filters');
    const params = new URLSearchParams({ page: String(shipmentPage), limit: '10' });
    if (form) {
      const values = new FormData(form);
      for (const [key, value] of values.entries()) if (value.trim()) params.set(key, value.trim());
    }
    return params.toString();
  }

  async function loadShipmentTable() {
    const target = document.querySelector('#shipment-table-panel');
    if (!target) return;
    target.innerHTML = '<div class="loading-block"><span><span class="spinner"></span>Loading shipments…</span></div>';
    try {
      const response = await API.get('/api/admin/shipments?' + queryFromFilters());
      target.innerHTML = shipmentTable(response.data.items || []) + pagination(response.data.pagination);
      target.querySelectorAll('.assign-shipment').forEach(button => button.addEventListener('click', () => openAssignModal(button.dataset.id, button.dataset.tracking)));
      target.querySelectorAll('.cancel-admin-shipment').forEach(button => button.addEventListener('click', async () => {
        if (!window.confirm('Cancel shipment ' + button.dataset.tracking + '? A permanent tracking event will be recorded.')) return;
        try {
          await API.put('/api/admin/shipments/' + button.dataset.id + '/status', { status: 'CANCELLED', remarks: 'Shipment cancelled by administrator' });
          UI.toast('Shipment cancelled.', 'success');
          await loadShipmentTable();
        } catch (error) { UI.toast(error.message, 'error'); }
      }));
      target.querySelectorAll('.pagination button[data-page]').forEach(button => button.addEventListener('click', () => { shipmentPage = Number(button.dataset.page); loadShipmentTable(); }));
    } catch (error) {
      target.innerHTML = UI.emptyState('Could not load shipments', error.message);
      UI.toast(error.message, 'error');
    }
  }

  function initShipments() {
    const filters = document.querySelector('#shipment-filters');
    if (!filters) return;
    filters.addEventListener('submit', event => { event.preventDefault(); shipmentPage = 1; loadShipmentTable(); });
    filters.addEventListener('reset', () => window.setTimeout(() => { shipmentPage = 1; loadShipmentTable(); }, 0));
    document.querySelector('#admin-create-shipment')?.addEventListener('click', () => {
      showModal('Create a new shipment', shipmentFormMarkup({}, true), 'Create shipment', async form => {
        const response = await API.post('/api/admin/shipments', objectFromShipmentForm(form));
        UI.toast('Shipment created. Tracking number: ' + response.data.tracking_number, 'success');
        shipmentPage = 1;
        await loadShipmentTable();
      });
    });
    loadShipmentTable();
  }

  function detailList(entries) {
    return '<dl class="detail-list">' + entries.map(([label, value]) => '<div><dt>' + e(label) + '</dt><dd>' + (value === undefined || value === null || value === '' ? '—' : e(value)) + '</dd></div>').join('') + '</dl>';
  }

  function detailTimeline(history) {
    return '<div class="timeline">' + (history || []).map(event => '<article class="timeline-event"><span class="timeline-dot"></span><div><div class="timeline-title"><strong>' + e(UI.statusLabel(event.status)) + '</strong><time class="timeline-time">' + UI.formatDate(event.created_at, true) + '</time></div><span class="timeline-location">' + e(event.location) + '</span><p>' + e(event.remarks) + '</p><span class="timeline-by">Updated by ' + e(event.updated_by) + '</span></div></article>').join('') + '</div>';
  }

  function statusActionMarkup(shipment, user) {
    const allowed = nextStates[shipment.current_status] || [];
    if (!allowed.length || (user.role !== 'ADMIN' && user.role !== 'STAFF')) return '';
    return '<section class="action-panel"><h2>Update shipment status</h2><p>All delivery stages are shown below. Only the next valid stage can be selected; each update is permanently recorded in customer tracking history.</p><form id="status-update-form" class="inline-form"><div class="form-field"><label>New status</label><select name="status" required><option value="" disabled selected>Select a status</option>' + shipmentStatuses.map(status => '<option value="' + status + '"' + (allowed.includes(status) ? '' : ' disabled') + '>' + e(UI.statusLabel(status)) + '</option>').join('') + '</select></div><div class="form-field"><label>Current location</label><input name="location" maxlength="120" placeholder="e.g. Ludhiana Hub"></div><div class="form-field"><label>Tracking remarks</label><input name="remarks" maxlength="500" placeholder="Add a useful update"></div><button class="button" type="submit">Save update</button></form></section>';
  }

  function adminControlMarkup(shipment) {
    const edit = !['DELIVERED', 'CANCELLED'].includes(shipment.current_status) ? '<button class="button button-outline" id="edit-shipment" type="button">Edit shipment</button>' : '';
    const remove = shipment.current_status === 'CANCELLED' ? '<button class="button button-danger" id="delete-shipment" type="button">Delete cancelled record</button>' : '';
    return '<section class="action-panel"><h2>Administrative controls</h2><p>Update shipment information or assign an active staff member. Only cancelled records can be permanently deleted.</p><div class="page-actions"><button class="button button-secondary" id="assign-from-detail" type="button">Assign staff</button>' + edit + remove + '</div></section>';
  }

  function customerControlMarkup(shipment) {
    return ['BOOKED', 'PICKED_UP', 'DELIVERY_FAILED'].includes(shipment.current_status)
      ? '<section class="action-panel"><h2>Need to cancel this shipment?</h2><p>Cancellations are available only before a shipment progresses beyond the permitted dispatch stages.</p><button class="button button-danger" id="cancel-shipment" type="button">Cancel shipment</button></section>' : '';
  }

  function renderDetail(shipment, user) {
    const target = document.querySelector('#shipment-detail-content');
    if (!target) return;
    target.innerHTML = '<section class="shipment-summary"><div><span class="summary-label">Tracking number</span><h1>' + e(shipment.tracking_number) + '</h1><span class="summary-label">Booked ' + UI.formatDate(shipment.booking_date) + '</span></div>' + UI.statusBadge(shipment.current_status) + '</section><div class="details-grid"><article class="detail-card"><h2>Sender information</h2>' + detailList([['Name', shipment.sender_name], ['Phone', shipment.sender_phone], ['Email', shipment.sender_email], ['Address', shipment.sender_address + ', ' + shipment.sender_city + ', ' + shipment.sender_state + ' - ' + shipment.sender_pincode]]) + '</article><article class="detail-card"><h2>Receiver information</h2>' + detailList([['Name', shipment.receiver_name], ['Phone', shipment.receiver_phone], ['Email', shipment.receiver_email], ['Address', shipment.receiver_address + ', ' + shipment.receiver_city + ', ' + shipment.receiver_state + ' - ' + shipment.receiver_pincode]]) + '</article><article class="detail-card"><h2>Package & service</h2>' + detailList([['Description', shipment.package_description], ['Weight', shipment.package_weight + ' kg'], ['Package type', shipment.package_type], ['Delivery type', shipment.delivery_type], ['Declared value', UI.currency(shipment.declared_value)], ['Shipping charge', UI.currency(shipment.shipping_charge)], ['Estimated delivery', UI.formatDate(shipment.estimated_delivery_date)], ['Assigned staff', shipment.assigned_staff_name || 'Unassigned']]) + '</article><article class="detail-card"><h2>Booking information</h2>' + detailList([['Customer', shipment.customer_name || 'Walk-in / Admin'], ['Booked on', UI.formatDate(shipment.booking_date, true)], ['Delivered at', shipment.delivered_at ? UI.formatDate(shipment.delivered_at, true) : 'Not delivered'], ['Current status', UI.statusLabel(shipment.current_status)]]) + '</article><article class="detail-card detail-wide"><h2>Tracking history</h2><div class="panel-body">' + (shipment.tracking_history && shipment.tracking_history.length ? detailTimeline(shipment.tracking_history) : UI.emptyState('No tracking events', 'Tracking history will appear here as the shipment moves.')) + '</div></article></div>' + statusActionMarkup(shipment, user) + (user.role === 'ADMIN' ? adminControlMarkup(shipment) : '') + (user.role === 'CUSTOMER' ? customerControlMarkup(shipment) : '');
    bindDetailActions(shipment, user);
  }

  function bindDetailActions(shipment, user) {
    document.querySelector('#status-update-form')?.addEventListener('submit', async event => {
      event.preventDefault();
      const form = event.currentTarget;
      const submit = form.querySelector('button[type="submit"]');
      UI.setBusy(submit, true, 'Saving…');
      try {
        const data = Object.fromEntries(new FormData(form).entries());
        const endpoint = user.role === 'ADMIN' ? '/api/admin/shipments/' + shipment.id + '/status' : '/api/staff/shipments/' + shipment.id + '/status';
        await API.put(endpoint, data);
        UI.toast('Shipment status updated.', 'success');
        loadDetail();
      } catch (error) { UI.toast(error.message, 'error'); }
      finally { UI.setBusy(submit, false); }
    });
    document.querySelector('#cancel-shipment')?.addEventListener('click', async () => {
      if (!window.confirm('Cancel this shipment? This creates a permanent tracking event.')) return;
      try { await API.post('/api/customer/shipments/' + shipment.id + '/cancel', {}); UI.toast('Shipment cancelled.', 'success'); loadDetail(); }
      catch (error) { UI.toast(error.message, 'error'); }
    });
    document.querySelector('#assign-from-detail')?.addEventListener('click', () => openAssignModal(shipment.id, shipment.tracking_number));
    document.querySelector('#delete-shipment')?.addEventListener('click', async () => {
      if (!window.confirm('Permanently delete this cancelled shipment and all of its tracking history?')) return;
      try {
        await API.delete('/api/admin/shipments/' + shipment.id);
        UI.toast('Cancelled shipment record deleted.', 'success');
        window.location.href = 'shipments.html';
      } catch (error) { UI.toast(error.message, 'error'); }
    });
    document.querySelector('#edit-shipment')?.addEventListener('click', () => {
      showModal('Edit ' + shipment.tracking_number, shipmentFormMarkup(shipment, false), 'Save changes', async form => {
        await API.put('/api/admin/shipments/' + shipment.id, objectFromShipmentForm(form));
        UI.toast('Shipment information updated.', 'success');
        await loadDetail();
      });
    });
  }

  async function loadDetail() {
    const id = new URLSearchParams(window.location.search).get('id');
    const target = document.querySelector('#shipment-detail-content');
    if (!id || !/^\d+$/.test(id)) { if (target) target.innerHTML = UI.emptyState('Shipment not selected', 'Open a shipment from a dashboard or management table.'); return; }
    if (target) target.innerHTML = '<div class="loading-block"><span><span class="spinner"></span>Loading shipment details…</span></div>';
    try {
      let endpoint;
      if (activeDetailUser.role === 'ADMIN') endpoint = '/api/admin/shipments/' + id;
      else if (activeDetailUser.role === 'CUSTOMER') endpoint = '/api/customer/shipments/' + id;
      else endpoint = '/api/staff/shipments/' + id;
      const response = await API.get(endpoint);
      renderDetail(response.data, activeDetailUser);
    } catch (error) {
      if (target) target.innerHTML = UI.emptyState('Unable to load shipment', error.message);
      UI.toast(error.message, 'error');
    }
  }

  async function initDetails(user) {
    activeDetailUser = user;
    loadDetail();
  }

  document.addEventListener('app:ready', event => {
    const page = document.body.dataset.page;
    if (page === 'shipments') initShipments();
    if (page === 'shipment-details') initDetails(event.detail.user);
  });
}());
