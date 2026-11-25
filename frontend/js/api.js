/* Same-origin API client. Session credentials remain in HttpOnly cookies. */
(function () {
  'use strict';

  class ApiError extends Error {
    constructor(message, status, response) {
      super(message);
      this.name = 'ApiError';
      this.status = status;
      this.response = response;
    }
  }

  async function request(path, options) {
    const settings = options || {};
    const headers = Object.assign({ Accept: 'application/json' }, settings.headers || {});
    const fetchOptions = {
      method: settings.method || 'GET',
      credentials: 'same-origin',
      headers: headers
    };
    if (settings.data !== undefined) {
      headers['Content-Type'] = 'application/json';
      fetchOptions.body = JSON.stringify(settings.data);
    }
    let response;
    let payload = null;
    try {
      response = await fetch(path, fetchOptions);
      const text = await response.text();
      payload = text ? JSON.parse(text) : null;
    } catch (error) {
      if (error instanceof SyntaxError) {
        throw new ApiError('The server returned an invalid response.', 0, null);
      }
      throw new ApiError('Unable to reach the server. Please check your connection and try again.', 0, null);
    }
    if (!response.ok || !payload || payload.success !== true) {
      throw new ApiError((payload && payload.message) || 'The request could not be completed.', response.status, payload);
    }
    return payload;
  }

  window.API = {
    request,
    get: (path) => request(path),
    post: (path, data) => request(path, { method: 'POST', data }),
    put: (path, data) => request(path, { method: 'PUT', data }),
    delete: (path) => request(path, { method: 'DELETE' }),
    ApiError
  };
}());
