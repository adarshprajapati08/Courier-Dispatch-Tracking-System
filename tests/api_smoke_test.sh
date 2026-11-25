#!/usr/bin/env bash
# Smoke tests for a running, demo-seeded Courier Dispatch & Tracking System.
# Usage:
# COURIER_TEST_ADMIN_EMAIL=admin@courier.demo \
# COURIER_TEST_ADMIN_PASSWORD='your-demo-password' ./tests/api_smoke_test.sh
set -euo pipefail

BASE_URL="${COURIER_TEST_BASE_URL:-http://localhost:8080}"
ADMIN_EMAIL="${COURIER_TEST_ADMIN_EMAIL:?Set COURIER_TEST_ADMIN_EMAIL}"
ADMIN_PASSWORD="${COURIER_TEST_ADMIN_PASSWORD:?Set COURIER_TEST_ADMIN_PASSWORD}"
COOKIE_JAR="$(mktemp)"
trap 'rm -f "$COOKIE_JAR"' EXIT

pass() { printf 'PASS  %s\n' "$1"; }
fail() { printf 'FAIL  %s\n' "$1" >&2; exit 1; }
json_success() { jq -e '.success == true' >/dev/null; }

status="$(curl -sS -o /tmp/courier_unauthorized.json -w '%{http_code}' "$BASE_URL/api/admin/dashboard")"
[[ "$status" == "401" ]] || fail "unauthenticated admin dashboard should return 401"
pass "Unauthenticated request is rejected"

curl -sS -c "$COOKIE_JAR" -H 'Content-Type: application/json' \
  -d "{\"email\":\"$ADMIN_EMAIL\",\"password\":\"$ADMIN_PASSWORD\",\"role\":\"ADMIN\"}" \
  "$BASE_URL/api/auth/login" | json_success || fail "admin login"
pass "Admin login"

curl -sS -b "$COOKIE_JAR" "$BASE_URL/api/auth/me" | jq -e '.success == true and .data.role == "ADMIN"' >/dev/null || fail "current session lookup"
pass "Session lookup"

curl -sS -b "$COOKIE_JAR" "$BASE_URL/api/admin/dashboard" | jq -e '.success == true and (.data.statistics.total_shipments >= 0)' >/dev/null || fail "dashboard query"
pass "Live admin dashboard query"

curl -sS -b "$COOKIE_JAR" "$BASE_URL/api/admin/shipments?limit=5" | jq -e '.success == true and (.data.pagination.total >= 0)' >/dev/null || fail "shipment list"
pass "Shipment list and pagination"

curl -sS -b "$COOKIE_JAR" "$BASE_URL/api/admin/reports" | jq -e '.success == true and (.data.summary.total_bookings >= 0)' >/dev/null || fail "reports query"
pass "Live reports query"

curl -sS "$BASE_URL/api/tracking/CDDEMO00001" | jq -e '.success == true and .data.tracking_number == "CDDEMO00001" and (.data.tracking_history | length >= 1)' >/dev/null || fail "public tracking"
pass "Public tracking timeline"

curl -sS -b "$COOKIE_JAR" -X POST "$BASE_URL/api/auth/logout" | json_success || fail "logout"
pass "Logout"
printf '\nAll non-destructive API smoke tests passed.\n'
