# Courier Package Dispatch and Tracking System

A full-stack **C17** university project that provides secure courier booking, operational dispatch management, public shipment tracking, staff delivery updates, and database-driven reporting.

> **Project name:** Courier Package Dispatch and Tracking System  
> **UI name:** SwiftRoute  
> **Server:** CivetWeb embedded C HTTP server  
> **Database:** SQLite3  
> **API format:** JSON over same-origin HTTP

---

## Objectives

- Demonstrate modular C programming in a practical full-stack application.
- Persist courier customers, shipments, tracking events, and sessions in SQLite.
- Provide role-based workflows for Customers, Administrators, and Staff.
- Generate secure credentials and sessions without storing plain-text passwords.
- Deliver a responsive, professional HTML/CSS/Vanilla JavaScript interface that communicates only with the C backend.

## Architecture

```text
HTML5 / CSS3 / Vanilla JavaScript
        │  fetch(), same origin, HttpOnly session cookie
        ▼
CivetWeb HTTP server (C17)
        │
        ├── server.c       REST routing and HTTP/JSON responses
        ├── auth.c         PBKDF2 passwords and hashed session tokens
        ├── shipment.c     booking, charges, assignment, transitions
        ├── tracking.c     public tracking and event timelines
        ├── user.c         customer/staff account management
        ├── admin.c        live dashboard/report queries
        └── SQLite3        users, shipments, tracking_history, sessions
```

The server serves both `/frontend` static files and `/api/...` from `http://localhost:8080`, so the frontend has no separate Node.js development server and does not need CORS configuration.

## Features

### Customer

- Registration, sign-in/sign-out, and profile updates.
- Complete courier booking form with sender, receiver, and package information.
- Backend-generated unique tracking number and booking confirmation.
- Configurable sample charge calculation: base charge + weight charge + 1% declared-value cover.
- Standard (estimated 5 days) and Express (estimated 2 days) service.
- Customer-only booking history, details, timeline, and eligible cancellation.
- Public tracking without login.

### Administrator

- Database-driven operations dashboard, status overview, recent shipments, and tracking updates.
- Shipment search, pagination, status/date filters, creation, editing, staff assignment, status updates, cancellation path, and safe deletion of cancelled records.
- Customer account search, booking-history view, deactivate/reactivate controls.
- Staff creation, editing, password reset, account-state management, and workload statistics.
- Reports for booking outcomes, delivery types, and monthly booking/delivery data.

### Staff

- Secure staff login and dashboard of assigned work.
- Tracking-ID search within assigned shipments.
- Complete package/detail/history view only for assigned packages.
- Valid status updates with location and tracking remarks.
- Backend blocks updates to shipments assigned to another staff member.

### Delivery tracking lifecycle

Staff and administrators open a shipment's details to see every delivery status in the update dropdown. The next valid status is selectable; other stages remain visible but disabled until the shipment reaches them:

1. Booked
2. Picked up
3. In transit
4. Out for delivery
5. Delivered

If delivery cannot be completed, staff or administrators can record **Delivery failed** from **In transit** or **Out for delivery**, then move it back to **Out for delivery** for another attempt. A shipment can be cancelled while **Booked**, **Picked up**, or **Delivery failed**. Status changes follow these valid transitions; completed and cancelled shipments are terminal.

Each update saves the shipment's current status and adds a tracking-history event with its location, remarks, timestamp, and staff/admin name. Customers see the current status and chronological events on shipment details and public tracking.

## Security

- Passwords use **PBKDF2-HMAC-SHA256** (210,000 iterations) with a random 128-bit salt via OpenSSL.
- Passwords, password hashes, salts, and raw session tokens are never returned in JSON.
- Sessions use a cryptographically random 256-bit token in an `HttpOnly; SameSite=Lax` cookie; the database stores only its SHA-256 hash.
- Sessions expire after seven days and inactive accounts cannot use an existing session.
- All user-controlled database values are bound through prepared SQLite statements.
- API handlers authenticate and authorize protected routes on every request.
- Shipment transition rules prevent invalid lifecycle changes.
- SQLite writes involving a shipment and tracking event use transactions.

For an HTTPS production deployment, terminate TLS in CivetWeb/reverse proxy and add the `Secure` cookie attribute.

## Technology Stack

| Layer | Technology |
|---|---|
| Frontend | HTML5, CSS3, Vanilla JavaScript, Fetch API |
| Backend | ISO C17, CivetWeb |
| Data | SQLite3 |
| JSON | cJSON |
| Cryptography | OpenSSL |
| Build | CMake |
| Charts | Chart.js browser library (reports page; graceful text fallback if unavailable) |

No Node.js, Python, PHP, Java, C++, JavaScript backend, or backend framework is used.

## Folder Structure

```text
Courier-Dispatch-Tracking-System/
├── CMakeLists.txt
├── README.md
├── .gitignore
├── frontend/
│   ├── index.html                 # Public landing page
│   ├── login.html / register.html
│   ├── book-courier.html / track.html
│   ├── customer-dashboard.html
│   ├── admin-dashboard.html / staff-dashboard.html
│   ├── shipments.html / shipment-details.html
│   ├── customers.html / staff.html / reports.html
│   ├── css/
│   │   ├── style.css
│   │   ├── dashboard.css
│   │   ├── forms.css
│   │   └── responsive.css
│   └── js/
│       ├── api.js / auth.js / main.js
│       ├── booking.js / tracking.js / dashboard.js
│       ├── shipments.js / customers.js / staff.js / reports.js
├── backend/
│   ├── include/
│   │   ├── server.h / database.h / auth.h / shipment.h / tracking.h
│   │   ├── user.h / admin.h / staff.h / validation.h / utils.h
│   ├── src/
│   │   ├── main.c / server.c / database.c / auth.c / shipment.c
│   │   ├── tracking.c / user.c / admin.c / staff.c
│   │   └── validation.c / utils.c
│   └── data/.gitkeep              # courier.db generated at runtime
└── tests/
    ├── README.md
    └── api_smoke_test.sh
```

## Database Design

| Table | Purpose | Main relationships |
|---|---|---|
| `users` | Customers, administrators, and staff | `role`, active/inactive state; referenced by shipments and sessions |
| `shipments` | Sender/receiver/package/service/current status | `customer_id → users`, `assigned_staff_id → users` |
| `tracking_history` | Immutable chronological events | `shipment_id → shipments` with cascade delete |
| `sessions` | Expiring hashed login tokens | `user_id → users` with cascade delete |

Important indexes exist for shipment customer/date, staff/status, status/date, tracking history chronology, user role/status, and session token/expiry lookup. Foreign keys are enabled on server startup.

### Shipment lifecycle

```text
BOOKED → PICKED_UP → IN_TRANSIT → OUT_FOR_DELIVERY → DELIVERED
                                          └────────→ DELIVERY_FAILED → OUT_FOR_DELIVERY or CANCELLED
BOOKED/PICKED_UP → CANCELLED
```

`DELIVERED` and `CANCELLED` are terminal states. Every permitted status change creates a `tracking_history` row with location, remarks, timestamp, and updater name.

## REST API

All routes return this consistent envelope:

```json
{ "success": true, "message": "...", "data": {} }
```

Errors return `{ "success": false, "message": "...", "data": null }` and an appropriate HTTP status.

### Authentication

| Method | Route |
|---|---|
| POST | `/api/auth/register` |
| POST | `/api/auth/login` |
| POST | `/api/auth/logout` |
| GET | `/api/auth/me` |

### Customer

| Method | Route |
|---|---|
| GET / PUT | `/api/customer/profile` |
| GET / POST | `/api/customer/shipments` |
| GET | `/api/customer/shipments/{id}` |
| POST | `/api/customer/shipments/{id}/cancel` |

### Public tracking

| Method | Route |
|---|---|
| GET | `/api/tracking/{tracking_number}` |

### Administrator

| Method | Route |
|---|---|
| GET | `/api/admin/dashboard` |
| GET / POST | `/api/admin/shipments` |
| GET / PUT / DELETE | `/api/admin/shipments/{id}` |
| PUT | `/api/admin/shipments/{id}/status` |
| PUT | `/api/admin/shipments/{id}/assign` |
| GET | `/api/admin/customers` |
| GET / PUT | `/api/admin/customers/{id}` *(extension for customer history/account state)* |
| GET / POST | `/api/admin/staff` |
| PUT | `/api/admin/staff/{id}` |
| GET | `/api/admin/reports` |

### Staff

| Method | Route |
|---|---|
| GET | `/api/staff/dashboard` |
| GET | `/api/staff/shipments` |
| GET | `/api/staff/shipments/{id}` *(assignment-authorized detail/history extension)* |
| PUT | `/api/staff/shipments/{id}/status` |

List endpoints support `page` and `limit`; shipment lists also support `search`, `status`, `date_from`, and `date_to` where applicable.

## Installation

### Linux (Debian/Ubuntu)

```bash
sudo apt update
sudo apt install build-essential cmake libcivetweb-dev libcjson-dev libsqlite3-dev libssl-dev curl jq
```

### Fedora/RHEL (package names can differ by release)

```bash
sudo dnf install gcc cmake civetweb-devel cjson-devel sqlite-devel openssl-devel curl jq
```

### Windows

Install CMake, a C17-capable compiler (Visual Studio Build Tools or MinGW-w64), and development builds of CivetWeb, cJSON, SQLite3, and OpenSSL. Make their headers/libraries discoverable through CMake, or set `CIVETWEB_INCLUDE_DIR`, `CIVETWEB_LIBRARY`, `CJSON_INCLUDE_DIR`, and `CJSON_LIBRARY` when configuring. The source uses Windows synchronization and time APIs under `_WIN32`.

## Build and Run

From the project root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./build/courier_server
```

Then open **http://localhost:8080/**.

Useful runtime options:

```bash
./build/courier_server --help
./build/courier_server --port 8090
./build/courier_server --database backend/data/my_courier.db
./build/courier_server --frontend frontend
```

The database is initialized automatically at `backend/data/courier.db` on first start.

## Secure Demo Data Setup

Demo accounts are **not embedded with a default password**. Seed them only into a new/empty database by choosing an explicit local-demo password:

```bash
rm -f backend/data/courier.db
export COURIER_DEMO_PASSWORD='ChooseYourOwnDemoPass1'
./build/courier_server --seed-demo
```

This creates:

| Role | Email |
|---|---|
| Administrator | `admin@courier.demo` |
| Staff | `aman.staff@courier.demo` |
| Staff | `neha.staff@courier.demo` |
| Customer | `riya.customer@courier.demo` |
| Customer | `arjun.customer@courier.demo` |
| Customer | `simran.customer@courier.demo` |

All demo users receive the value you supplied in `COURIER_DEMO_PASSWORD`. The seed also creates ten shipments across multiple statuses/months and multiple chronological tracking records. **Change the password and demo account details before any use beyond local demonstration.**

## Testing

### Completed verification during development

- CMake configuration and C17 build completed successfully with GCC.
- Server successfully served static frontend files and API routes at port 8080.
- Demo seeding created users, 10 shipments, status diversity, tracking events, and multi-month report data.
- Tested public tracking, customer registration/login/booking/cancellation, admin dashboard/list/report queries, customer ownership protection, staff assignment restriction, valid staff status update, invalid reverse transition rejection, customer detail/history, and unauthenticated rejection.
- All frontend JavaScript files passed syntax checking; 13 required HTML pages are present.

### Repeatable smoke test

After seeding and starting the server:

```bash
chmod +x tests/api_smoke_test.sh
COURIER_TEST_ADMIN_EMAIL=admin@courier.demo \
COURIER_TEST_ADMIN_PASSWORD='ChooseYourOwnDemoPass1' \
./tests/api_smoke_test.sh
```

See `tests/README.md` for details.

## C Programming Concepts Demonstrated

- Modular `.c` / `.h` design with header guards and cohesive modules.
- Structures (`Database`, `AuthUser`, `ShipmentFilters`, `AppContext`) and pointer-based APIs.
- Dynamic memory for HTTP request bodies and cJSON trees with explicit freeing.
- Thread-aware database synchronization around multi-statement transactions.
- Prepared SQLite statements and explicit error/return-value handling.
- OpenSSL random bytes, SHA-256, and PBKDF2 primitives.
- Enumerated user roles, validation functions, state transitions, and role-based authorization.
- CivetWeb callback routing and static-file hosting.

## Troubleshooting

| Symptom | Check |
|---|---|
| CMake cannot find a library | Install the corresponding `-dev` package or provide CMake include/library variables. |
| Port 8080 is busy | Run with `--port 8090` and open that port in the browser. |
| Login has no administrator | Seed a new empty database using `--seed-demo` and `COURIER_DEMO_PASSWORD`. |
| Demo seed refuses to run | It intentionally refuses databases containing any users. Remove/use a fresh DB first. |
| Frontend looks unstyled | Always use the C server URL, not `file://`; CivetWeb serves the relative CSS/JS paths. |
| Charts show text instead of graphics | The reports page gracefully falls back if Chart.js CDN cannot load; data still comes from the C API. |

## Future Enhancements

- Email/SMS notifications and webhook integrations.
- Proof-of-delivery image upload/storage.
- Multi-branch hubs and route optimization.
- Rate-zone configuration managed through an administrator UI.
- CSV/PDF report export.
- HTTPS configuration and production cookie `Secure` flag.
- Fine-grained audit logs and password-change workflow.


admin email and password:
email: admin@courier.demo
password: DemoPass123
| Staff | `aman.staff@courier.demo` |
| Staff | `neha.staff@courier.demo` |

run commands:

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
.\build\courier_server.exe --port 8090

.\build\courier_server.exe
