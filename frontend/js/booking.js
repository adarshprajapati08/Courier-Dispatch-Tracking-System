(function () {
  'use strict';

  function calculatePreview() {
    const weight = Number(document.querySelector('[name="package_weight"]')?.value || 0);
    const declared = Number(document.querySelector('[name="declared_value"]')?.value || 0);
    const type = document.querySelector('[name="delivery_type"]')?.value || 'Standard';
    const amount = (type === 'Express' ? 140 + weight * 60 : 70 + weight * 35) + declared * .01;
    const output = document.querySelector('#charge-preview');
    const estimate = document.querySelector('#delivery-preview');
    if (output) output.textContent = UI.currency(Math.max(0, amount));
    if (estimate) estimate.textContent = type === 'Express' ? '2 business days' : '5 business days';
  }

  function confirmation(data) {
    const target = document.querySelector('#booking-area');
    if (!target) return;
    target.innerHTML = '<section class="card booking-confirmation"><div class="confirm-mark">✓</div><h2>Courier booked successfully</h2><p>Your shipment is confirmed and ready for pickup. Keep the tracking number below to follow every movement.</p><div class="tracking-id-box"><span>Tracking number</span><strong>' + UI.escapeHtml(data.tracking_number) + '</strong><button class="button button-small button-outline" id="copy-tracking" type="button">Copy tracking number</button></div><div class="booking-meta"><div><small>Shipping charge</small><strong>' + UI.currency(data.shipping_charge) + '</strong></div><div><small>Estimated delivery</small><strong>' + UI.formatDate(data.estimated_delivery_date) + '</strong></div></div><div class="hero-actions" style="justify-content:center"><a class="button" href="track.html?tracking=' + encodeURIComponent(data.tracking_number) + '">Track shipment</a><a class="button button-outline" href="customer-dashboard.html">Go to dashboard</a></div></section>';
    document.querySelector('#copy-tracking').addEventListener('click', async () => {
      try { await navigator.clipboard.writeText(data.tracking_number); UI.toast('Tracking number copied.', 'success'); }
      catch (_) { UI.toast('Copy this tracking number: ' + data.tracking_number, 'info'); }
    });
  }

  async function init() {
    const form = document.querySelector('#booking-form');
    if (!form) return;
    const user = await Auth.requireAuth(['CUSTOMER']);
    if (!user) return;
    const senderName = form.querySelector('[name="sender_name"]');
    const senderPhone = form.querySelector('[name="sender_phone"]');
    const senderEmail = form.querySelector('[name="sender_email"]');
    if (senderName && !senderName.value) senderName.value = user.name;
    if (senderPhone && !senderPhone.value) senderPhone.value = user.phone;
    if (senderEmail && !senderEmail.value) senderEmail.value = user.email;
    form.querySelectorAll('[name="package_weight"],[name="declared_value"],[name="delivery_type"]').forEach(input => input.addEventListener('input', calculatePreview));
    calculatePreview();
    form.addEventListener('submit', async (event) => {
      event.preventDefault();
      const submit = form.querySelector('button[type="submit"]');
      const payload = Object.fromEntries(new FormData(form).entries());
      payload.package_weight = Number(payload.package_weight);
      payload.declared_value = Number(payload.declared_value || 0);
      UI.setBusy(submit, true, 'Booking courier…');
      try {
        const response = await API.post('/api/customer/shipments', payload);
        confirmation(response.data);
        window.scrollTo({ top: 0, behavior: 'smooth' });
      } catch (error) {
        UI.toast(error.message, 'error');
      } finally {
        UI.setBusy(submit, false);
      }
    });
  }

  document.addEventListener('app:ready', init);
}());
