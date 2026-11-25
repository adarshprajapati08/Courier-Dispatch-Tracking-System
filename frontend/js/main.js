/* Shared navigation, presentation, notifications, and safe DOM utilities. */
(function () {
  'use strict';

  const publicLinks = [
    ['index.html', 'Home'],
    ['book-courier.html', 'Book a courier'],
    ['track.html', 'Track shipment']
  ];

  const appLinks = {
    CUSTOMER: [
      ['customer-dashboard.html', 'Overview', 'grid'],
      ['book-courier.html', 'Book courier', 'plus'],
      ['customer-dashboard.html#shipments', 'My shipments', 'box']
    ],
    ADMIN: [
      ['admin-dashboard.html', 'Dashboard', 'grid'],
      ['shipments.html', 'Shipments', 'box'],
      ['customers.html', 'Customers', 'users'],
      ['staff.html', 'Staff', 'briefcase'],
      ['reports.html', 'Reports', 'chart']
    ],
    STAFF: [
      ['staff-dashboard.html', 'My dashboard', 'grid'],
      ['staff-dashboard.html#shipments', 'Assigned shipments', 'box']
    ]
  };

  const icon = (name) => {
    const paths = {
      grid: '<rect x="3" y="3" width="7" height="7" rx="1"></rect><rect x="14" y="3" width="7" height="7" rx="1"></rect><rect x="3" y="14" width="7" height="7" rx="1"></rect><rect x="14" y="14" width="7" height="7" rx="1"></rect>',
      plus: '<path d="M12 5v14M5 12h14"></path>',
      box: '<path d="M21 8a2 2 0 0 0-1-1.73l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.73l7 4a2 2 0 0 0 2 0l7-4A2 2 0 0 0 21 16Z"></path><path d="m3.3 7 8.7 5 8.7-5M12 22V12"></path>',
      users: '<path d="M16 21v-2a4 4 0 0 0-4-4H6a4 4 0 0 0-4 4v2"></path><circle cx="9" cy="7" r="4"></circle><path d="M22 21v-2a4 4 0 0 0-3-3.87M16 3.13a4 4 0 0 1 0 7.75"></path>',
      briefcase: '<rect x="3" y="7" width="18" height="13" rx="2"></rect><path d="M8 7V5a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2M3 12h18M10 12v2h4v-2"></path>',
      chart: '<path d="M4 19V5M4 19h16"></path><path d="m7 15 4-4 3 2 5-6"></path>'
    };
    return '<svg viewBox="0 0 24 24" aria-hidden="true">' + (paths[name] || paths.grid) + '</svg>';
  };

  function escapeHtml(value) {
    return String(value === null || value === undefined ? '' : value)
      .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;').replace(/'/g, '&#039;');
  }

  function formatDate(value, withTime) {
    if (!value) return '—';
    const parsed = new Date(value);
    if (Number.isNaN(parsed.getTime())) return escapeHtml(value);
    return new Intl.DateTimeFormat('en-IN', withTime
      ? { dateStyle: 'medium', timeStyle: 'short' }
      : { dateStyle: 'medium' }).format(parsed);
  }

  function currency(value) {
    return new Intl.NumberFormat('en-IN', { style: 'currency', currency: 'INR', maximumFractionDigits: 2 }).format(Number(value || 0));
  }

  function statusLabel(status) {
    return String(status || '').replaceAll('_', ' ').replace(/\b\w/g, c => c.toUpperCase()) || 'Unknown';
  }

  function statusBadge(status) {
    return '<span class="status status-' + escapeHtml(String(status || '').toLowerCase()) + '">' + escapeHtml(statusLabel(status)) + '</span>';
  }

  function toast(message, type) {
    const holder = document.querySelector('#toast-holder') || (() => {
      const element = document.createElement('div');
      element.id = 'toast-holder';
      element.className = 'toast-holder';
      document.body.appendChild(element);
      return element;
    })();
    const notification = document.createElement('div');
    notification.className = 'toast toast-' + (type || 'info');
    notification.setAttribute('role', 'status');
    notification.innerHTML = '<span class="toast-mark">' + (type === 'success' ? '✓' : type === 'error' ? '!' : 'i') + '</span><span></span><button type="button" aria-label="Dismiss">×</button>';
    notification.querySelector('span:nth-child(2)').textContent = message;
    notification.querySelector('button').addEventListener('click', () => notification.remove());
    holder.appendChild(notification);
    window.setTimeout(() => notification.remove(), 5000);
  }

  function setBusy(button, busy, text) {
    if (!button) return;
    if (busy) {
      button.dataset.originalLabel = button.innerHTML;
      button.disabled = true;
      button.innerHTML = '<span class="spinner"></span>' + (text || 'Please wait…');
    } else {
      button.disabled = false;
      button.innerHTML = button.dataset.originalLabel || button.innerHTML;
    }
  }

  function emptyState(title, detail) {
    return '<div class="empty-state"><div class="empty-icon">⌁</div><h3>' + escapeHtml(title) + '</h3><p>' + escapeHtml(detail || '') + '</p></div>';
  }

  function publicNavigation() {
    const current = document.body.dataset.page || '';
    const linkHtml = publicLinks.map(([href, label]) => '<a class="' + (current === href.replace('.html', '') ? 'active' : '') + '" href="' + href + '">' + label + '</a>').join('');
    return '<header class="public-header"><a class="brand" href="index.html" aria-label="SwiftRoute home"><span class="brand-mark">↗</span><span>SwiftRoute</span></a><button class="nav-toggle" aria-label="Open navigation" type="button">☰</button><nav class="public-nav">' + linkHtml + '<a class="button button-small button-outline" href="login.html">Sign in</a><a class="button button-small" href="register.html">Create account</a></nav></header>';
  }

  function appNavigation(user) {
    const current = document.body.dataset.page || '';
    const links = (appLinks[user.role] || []).map(([href, label, iconName]) => {
      const plain = href.split('#')[0].replace('.html', '');
      return '<a href="' + href + '" class="sidebar-link ' + (current === plain ? 'active' : '') + '">' + icon(iconName) + '<span>' + label + '</span></a>';
    }).join('');
    return '<div class="app-layout"><aside class="sidebar"><a class="brand sidebar-brand" href="' + Auth.dashboardFor(user.role) + '"><span class="brand-mark">↗</span><span>SwiftRoute</span></a><div class="sidebar-role">' + escapeHtml(user.role.toLowerCase()) + ' portal</div><nav class="sidebar-nav">' + links + '</nav><div class="sidebar-footer"><div class="user-mini"><span class="avatar">' + escapeHtml(user.name.charAt(0).toUpperCase()) + '</span><div><strong>' + escapeHtml(user.name) + '</strong><small>' + escapeHtml(user.email) + '</small></div></div><button type="button" class="sidebar-link logout-button">' + icon('plus') + '<span>Sign out</span></button></div></aside><main class="app-main"><header class="app-header"><button class="sidebar-toggle" aria-label="Open menu" type="button">☰</button><div class="app-header-spacer"></div><a class="header-track-link" href="track.html">Track a parcel</a><span class="avatar header-avatar">' + escapeHtml(user.name.charAt(0).toUpperCase()) + '</span></header><div class="page-content" id="page-content"></div></main></div>';
  }

  function publicFooter() {
    return '<div class="footer-top"><div><a class="brand" href="index.html"><span class="brand-mark">↗</span><span>SwiftRoute</span></a><p>Reliable booking, dispatch visibility, and transparent parcel tracking in one professional courier platform.</p></div><nav class="footer-links"><a href="book-courier.html">Book courier</a><a href="track.html">Track shipment</a><a href="login.html">Sign in</a></nav></div><div class="footer-bottom">© 2026 SwiftRoute Courier Services · University project demonstration platform</div>';
  }

  function enablePublicNav() {
    const toggle = document.querySelector('.nav-toggle');
    const nav = document.querySelector('.public-nav');
    if (toggle && nav) toggle.addEventListener('click', () => nav.classList.toggle('open'));
  }

  function enableAppNav() {
    const toggle = document.querySelector('.sidebar-toggle');
    const sidebar = document.querySelector('.sidebar');
    if (toggle && sidebar) toggle.addEventListener('click', () => sidebar.classList.toggle('open'));
    const logout = document.querySelector('.logout-button');
    if (logout) logout.addEventListener('click', Auth.signOut);
  }

  async function boot() {
    const shell = document.querySelector('#site-shell');
    const layout = document.body.dataset.layout || 'public';
    if (!shell) return;
    if (layout === 'public') {
      shell.innerHTML = publicNavigation();
      const footer = document.querySelector('#site-footer');
      if (footer) { footer.className = 'site-footer'; footer.innerHTML = publicFooter(); }
      enablePublicNav();
      document.querySelectorAll('[data-track-redirect]').forEach(form => form.addEventListener('submit', event => {
        event.preventDefault();
        const input = form.querySelector('input[name="tracking"]');
        const tracking = input && input.value.trim();
        if (!tracking) { toast('Enter a tracking number to continue.', 'error'); return; }
        window.location.href = 'track.html?tracking=' + encodeURIComponent(tracking.toUpperCase());
      }));
      return;
    }
    const required = (document.body.dataset.roles || '').split(',').filter(Boolean);
    const user = await Auth.requireAuth(required.length ? required : null);
    if (!user) return;
    shell.innerHTML = appNavigation(user);
    enableAppNav();
    const template = document.querySelector('#page-template');
    const destination = document.querySelector('#page-content');
    if (template && destination) destination.innerHTML = template.innerHTML;
    document.dispatchEvent(new CustomEvent('app:ready', { detail: { user } }));
  }

  window.UI = { escapeHtml, formatDate, currency, statusLabel, statusBadge, toast, setBusy, emptyState, icon, boot };
  document.addEventListener('DOMContentLoaded', boot);
}());
