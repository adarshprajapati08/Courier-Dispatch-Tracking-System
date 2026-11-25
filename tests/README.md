# API testing

`api_smoke_test.sh` performs non-destructive smoke tests against a **running, demo-seeded** server:

```bash
chmod +x tests/api_smoke_test.sh
COURIER_TEST_ADMIN_EMAIL=admin@courier.demo \
COURIER_TEST_ADMIN_PASSWORD='ChooseYourOwnDemoPass1' \
./tests/api_smoke_test.sh
```

Optional base URL override:

```bash
COURIER_TEST_BASE_URL=http://localhost:8090 \
COURIER_TEST_ADMIN_EMAIL=admin@courier.demo \
COURIER_TEST_ADMIN_PASSWORD='ChooseYourOwnDemoPass1' \
./tests/api_smoke_test.sh
```

The script validates unauthenticated rejection, admin authentication, current-session lookup, live dashboard/report queries, shipment list pagination, public tracking, and logout. It deliberately does not create, modify, or delete production records.

For manual role/authorization checks, sign in with the customer and staff accounts created during demo seeding, then verify that a customer cannot open another customer's shipment and a staff member cannot update a shipment assigned to someone else.
