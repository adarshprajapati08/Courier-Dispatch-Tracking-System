(function () {
  'use strict';
  let page = 1;

  function modal(title, html) {
    const back = document.createElement('div');
    back.className = 'modal-backdrop';
    back.innerHTML = '<div class="modal" role="dialog" aria-modal="true"><header class="modal-head"><h2>' + UI.escapeHtml(title) + '</h2><button class="modal-close" type="button">×</button></header><div class="modal-body">' + html + '</div></div>';
    document.body.appendChild(back);
    const close = () => back.remove();
    back.querySelector('.modal-close').addEventListener('click', close);
    back.addEventListener('click', event => { if (event.target === back) close(); });
  }

  function profileView(data) {
    const profile = data.profile;
    const shipments = data.shipments.items || [];
    const rows = shipments.length ? shipments.map(s => '<tr><td><a class="table-link" href="shipment-details.html?id=' + s.id + '">' + UI.escapeHtml(s.tracking_number) + '</a></td><td>' + UI.escapeHtml(s.receiver_name) + '<span class="table-sub">' + UI.escapeHtml(s.receiver_city) + '</span></td><td>' + UI.statusBadge(s.current_status) + '</td><td>' + UI.formatDate(s.booking_date) + '</td></tr>').join('') : '<tr><td colspan="4">No bookings for this customer.</td></tr>';
    return '<div class="details-grid" style="margin-bottom:18px"><div class="detail-card"><h2>Customer profile</h2><dl class="detail-list"><div><dt>Name</dt><dd>' + UI.escapeHtml(profile.name) + '</dd></div><div><dt>Email</dt><dd>' + UI.escapeHtml(profile.email) + '</dd></div><div><dt>Phone</dt><dd>' + UI.escapeHtml(profile.phone) + '</dd></div><div><dt>Status</dt><dd>' + UI.statusBadge(profile.account_status === 'ACTIVE' ? 'DELIVERED' : 'CANCELLED') + ' ' + UI.escapeHtml(profile.account_status) + '</dd></div></dl></div><div class="detail-card"><h2>Booking summary</h2><dl class="detail-list"><div><dt>Total bookings</dt><dd>' + profile.shipment_count + '</dd></div><div><dt>Delivered</dt><dd>' + profile.delivered_count + '</dd></div><div><dt>Joined</dt><dd>' + UI.formatDate(profile.created_at) + '</dd></div></dl></div></div><h3 class="mini-form-title">Booking history</h3><div class="table-wrap"><table class="data-table"><thead><tr><th>Tracking</th><th>Receiver</th><th>Status</th><th>Booked</th></tr></thead><tbody>' + rows + '</tbody></table></div>';
  }

  async function showCustomer(id) {
    try {
      const response = await API.get('/api/admin/customers/' + id);
      modal('Customer details', profileView(response.data));
    } catch (error) { UI.toast(error.message, 'error'); }
  }

  async function updateStatus(customer) {
    const next = customer.account_status === 'ACTIVE' ? 'INACTIVE' : 'ACTIVE';
    const wording = next === 'INACTIVE' ? 'deactivate' : 'reactivate';
    if (!window.confirm('Do you want to ' + wording + ' ' + customer.name + '\'s account?')) return;
    try {
      await API.put('/api/admin/customers/' + customer.id, { account_status: next });
      UI.toast('Customer account ' + wording + 'd.', 'success');
      load();
    } catch (error) { UI.toast(error.message, 'error'); }
  }

  function render(data) {
    const target = document.querySelector('#customers-panel');
    const items = data.items || [];
    const rows = items.length ? items.map(c => '<tr><td><span class="table-main">' + UI.escapeHtml(c.name) + '</span><span class="table-sub">Joined ' + UI.formatDate(c.created_at) + '</span></td><td>' + UI.escapeHtml(c.email) + '<span class="table-sub">' + UI.escapeHtml(c.phone) + '</span></td><td>' + c.shipment_count + '</td><td>' + c.delivered_count + '</td><td>' + (c.account_status === 'ACTIVE' ? '<span class="status status-delivered">Active</span>' : '<span class="status status-cancelled">Inactive</span>') + '</td><td><div class="table-actions"><button class="small-action customer-view" data-id="' + c.id + '">View</button><button class="small-action customer-toggle" data-id="' + c.id + '" data-name="' + UI.escapeHtml(c.name) + '" data-status="' + c.account_status + '">' + (c.account_status === 'ACTIVE' ? 'Deactivate' : 'Reactivate') + '</button></div></td></tr>').join('') : '<tr><td colspan="6">' + UI.emptyState('No customers found', 'Try another search phrase.') + '</td></tr>';
    const meta = data.pagination;
    target.innerHTML = '<div class="table-wrap"><table class="data-table"><thead><tr><th>Customer</th><th>Contact</th><th>Bookings</th><th>Delivered</th><th>Account</th><th>Actions</th></tr></thead><tbody>' + rows + '</tbody></table></div><footer class="pagination"><span>Page ' + meta.page + ' of ' + Math.max(1, meta.total_pages) + ' · ' + meta.total + ' customer' + (meta.total === 1 ? '' : 's') + '</span><div class="pagination-controls"><button data-page="' + (meta.page - 1) + '"' + (meta.page <= 1 ? ' disabled' : '') + '>‹</button><button data-page="' + (meta.page + 1) + '"' + (meta.page >= meta.total_pages || !meta.total_pages ? ' disabled' : '') + '>›</button></div></footer>';
    target.querySelectorAll('.customer-view').forEach(button => button.addEventListener('click', () => showCustomer(button.dataset.id)));
    target.querySelectorAll('.customer-toggle').forEach(button => button.addEventListener('click', () => updateStatus({ id: button.dataset.id, name: button.dataset.name, account_status: button.dataset.status })));
    target.querySelectorAll('.pagination button[data-page]').forEach(button => button.addEventListener('click', () => { page = Number(button.dataset.page); load(); }));
  }

  async function load() {
    const target = document.querySelector('#customers-panel');
    const form = document.querySelector('#customer-filters');
    if (!target) return;
    target.innerHTML = '<div class="loading-block"><span><span class="spinner"></span>Loading customers…</span></div>';
    const params = new URLSearchParams({ page: String(page), limit: '10' });
    const search = form?.querySelector('[name="search"]')?.value.trim();
    if (search) params.set('search', search);
    try { render((await API.get('/api/admin/customers?' + params)).data); }
    catch (error) { target.innerHTML = UI.emptyState('Could not load customers', error.message); UI.toast(error.message, 'error'); }
  }

  document.addEventListener('app:ready', () => {
    if (document.body.dataset.page !== 'customers') return;
    document.querySelector('#customer-filters').addEventListener('submit', event => { event.preventDefault(); page = 1; load(); });
    load();
  });
}());
