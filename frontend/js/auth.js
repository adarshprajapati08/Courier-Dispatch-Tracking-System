/* Authentication helpers. No token is copied to localStorage or sessionStorage. */
(function () {
  'use strict';

  const rolePages = {
    ADMIN: 'admin-dashboard.html',
    STAFF: 'staff-dashboard.html',
    CUSTOMER: 'customer-dashboard.html'
  };

  function dashboardFor(role) {
    return rolePages[role] || 'index.html';
  }

  async function getCurrentUser() {
    const response = await API.get('/api/auth/me');
    window.currentUser = response.data;
    return response.data;
  }

  async function requireAuth(roles) {
    try {
      const user = await getCurrentUser();
      if (roles && !roles.includes(user.role)) {
        window.location.replace(dashboardFor(user.role));
        return null;
      }
      return user;
    } catch (error) {
      window.location.replace('login.html?next=' + encodeURIComponent(window.location.pathname.split('/').pop() || 'index.html'));
      return null;
    }
  }

  async function signOut() {
    try { await API.post('/api/auth/logout', {}); } catch (error) { /* clear cookie response is still handled server-side where possible */ }
    window.currentUser = null;
    window.location.replace('index.html');
  }

  function validPassword(password) {
    return password.length >= 8 && /[a-z]/.test(password) && /[A-Z]/.test(password) && /\d/.test(password);
  }

  function initLogin() {
    const form = document.querySelector('#login-form');
    if (!form) return;
    form.addEventListener('submit', async (event) => {
      event.preventDefault();
      const submit = form.querySelector('button[type="submit"]');
      const data = Object.fromEntries(new FormData(form).entries());
      UI.setBusy(submit, true, 'Signing in…');
      try {
        const response = await API.post('/api/auth/login', data);
        window.currentUser = response.data;
        UI.toast('Welcome back, ' + response.data.name + '.', 'success');
        const next = new URLSearchParams(window.location.search).get('next');
        const safeNext = next && !next.includes('://') && !next.startsWith('//') ? next : dashboardFor(response.data.role);
        window.setTimeout(() => window.location.replace(safeNext), 300);
      } catch (error) {
        UI.toast(error.message, 'error');
      } finally {
        UI.setBusy(submit, false);
      }
    });
  }

  function initRegister() {
    const form = document.querySelector('#register-form');
    if (!form) return;
    form.addEventListener('submit', async (event) => {
      event.preventDefault();
      const submit = form.querySelector('button[type="submit"]');
      const data = Object.fromEntries(new FormData(form).entries());
      if (data.password !== data.confirm_password) {
        UI.toast('Password confirmation does not match.', 'error');
        return;
      }
      if (!validPassword(data.password)) {
        UI.toast('Use 8+ characters with uppercase, lowercase, and a number.', 'error');
        return;
      }
      UI.setBusy(submit, true, 'Creating account…');
      try {
        await API.post('/api/auth/register', data);
        UI.toast('Your account is ready. Please sign in.', 'success');
        window.setTimeout(() => window.location.replace('login.html'), 500);
      } catch (error) {
        UI.toast(error.message, 'error');
      } finally {
        UI.setBusy(submit, false);
      }
    });
  }

  window.Auth = { getCurrentUser, requireAuth, signOut, dashboardFor, initLogin, initRegister };
}());
