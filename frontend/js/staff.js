(function () {
  'use strict';
  let page = 1;

  function openFormModal(title, staff, create) {
    const s = staff || {};
    const backdrop = document.createElement('div');
    backdrop.className = 'modal-backdrop';
    backdrop.innerHTML = '<div class="modal" role="dialog" aria-modal="true"><form id="staff-form"><header class="modal-head"><h2>' + UI.escapeHtml(title) + '</h2><button type="button" class="modal-close">×</button></header><div class="modal-body"><div class="form-grid"><div class="form-field"><label>Full name <span class="required">*</span></label><input name="name" required value="' + UI.escapeHtml(s.name || '') + '"></div><div class="form-field"><label>Phone <span class="required">*</span></label><input name="phone" required value="' + UI.escapeHtml(s.phone || '') + '"></div><div class="form-field span-2"><label>Email address <span class="required">*</span></label><input type="email" name="email" required value="' + UI.escapeHtml(s.email || '') + '"></div>' + (create ? '<div class="form-field span-2"><label>Temporary password <span class="required">*</span></label><input type="password" name="password" minlength="8" required><small>Must include uppercase, lowercase, and a number. Share securely with the staff member.</small></div>' : '<div class="form-field span-2"><label>New password <small>(Optional — leave blank to keep the current password)</small></label><input type="password" name="password" minlength="8"></div>') + '<div class="form-field"><label>Account status</label><select name="account_status"><option value="ACTIVE"' + ((s.account_status || 'ACTIVE') === 'ACTIVE' ? ' selected' : '') + '>Active</option><option value="INACTIVE"' + (s.account_status === 'INACTIVE' ? ' selected' : '') + '>Inactive</option></select></div></div></div><footer class="modal-foot"><button type="button" class="button button-outline modal-close">Cancel</button><button class="button" type="submit">' + (create ? 'Create staff account' : 'Save staff changes') + '</button></footer></form></div>';
    document.body.appendChild(backdrop);
    const close = () => backdrop.remove();
    backdrop.querySelectorAll('.modal-close').forEach(btn => btn.addEventListener('click', close));
    const form = backdrop.querySelector('#staff-form');
    form.addEventListener('submit', async event => {
      event.preventDefault();
      const button = form.querySelector('button[type="submit"]');
      const data = Object.fromEntries(new FormData(form).entries());
      if (!data.password) delete data.password;
      if (data.password && !(data.password.length >= 8 && /[A-Z]/.test(data.password) && /[a-z]/.test(data.password) && /\d/.test(data.password))) { UI.toast('Password needs uppercase, lowercase, and a number.', 'error'); return; }
      UI.setBusy(button, true, create ? 'Creating…' : 'Saving…');
      try {
        if (create) await API.post('/api/admin/staff', data);
        else await API.put('/api/admin/staff/' + s.id, data);
        UI.toast(create ? 'Staff account created.' : 'Staff account updated.', 'success');
        close(); page = 1; load();
      } catch (error) { UI.toast(error.message, 'error'); }
      finally { UI.setBusy(button, false); }
    });
  }

  function render(data) {
    const target = document.querySelector('#staff-panel');
    const items = data.items || [];
    const rows = items.length ? items.map(s => '<tr><td><span class="table-main">' + UI.escapeHtml(s.name) + '</span><span class="table-sub">Joined ' + UI.formatDate(s.created_at) + '</span></td><td>' + UI.escapeHtml(s.email) + '<span class="table-sub">' + UI.escapeHtml(s.phone) + '</span></td><td>' + s.shipment_count + '</td><td>' + s.delivered_count + '</td><td>' + (s.account_status === 'ACTIVE' ? '<span class="status status-delivered">Active</span>' : '<span class="status status-cancelled">Inactive</span>') + '</td><td><button class="small-action staff-edit" data-id="' + s.id + '">Edit</button></td></tr>').join('') : '<tr><td colspan="6">' + UI.emptyState('No staff members found', 'Create a staff account to begin assigning shipments.') + '</td></tr>';
    const meta = data.pagination;
    target.innerHTML = '<div class="table-wrap"><table class="data-table"><thead><tr><th>Staff member</th><th>Contact</th><th>Assigned</th><th>Delivered</th><th>Account</th><th>Actions</th></tr></thead><tbody>' + rows + '</tbody></table></div><footer class="pagination"><span>Page ' + meta.page + ' of ' + Math.max(meta.total_pages,1) + ' · ' + meta.total + ' staff member' + (meta.total === 1 ? '' : 's') + '</span><div class="pagination-controls"><button data-page="' + (meta.page - 1) + '"' + (meta.page <= 1 ? ' disabled' : '') + '>‹</button><button data-page="' + (meta.page + 1) + '"' + (meta.page >= meta.total_pages || !meta.total_pages ? ' disabled' : '') + '>›</button></div></footer>';
    target.querySelectorAll('.staff-edit').forEach(button => button.addEventListener('click', () => {
      const staff = items.find(item => String(item.id) === button.dataset.id);
      if (staff) openFormModal('Edit staff member', staff, false);
    }));
    target.querySelectorAll('.pagination button[data-page]').forEach(button => button.addEventListener('click', () => { page = Number(button.dataset.page); load(); }));
  }

  async function load() {
    const target = document.querySelector('#staff-panel');
    const filter = document.querySelector('#staff-filters [name="search"]')?.value.trim();
    const params = new URLSearchParams({ page: String(page), limit: '10' });
    if (filter) params.set('search', filter);
    target.innerHTML = '<div class="loading-block"><span><span class="spinner"></span>Loading staff…</span></div>';
    try { render((await API.get('/api/admin/staff?' + params)).data); }
    catch (error) { target.innerHTML = UI.emptyState('Could not load staff', error.message); UI.toast(error.message, 'error'); }
  }

  document.addEventListener('app:ready', () => {
    if (document.body.dataset.page !== 'staff') return;
    document.querySelector('#staff-filters').addEventListener('submit', event => { event.preventDefault(); page = 1; load(); });
    document.querySelector('#add-staff').addEventListener('click', () => openFormModal('Add staff member', null, true));
    load();
  });
}());
