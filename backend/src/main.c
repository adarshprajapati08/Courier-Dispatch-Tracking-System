#include "database.h"
#include "server.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

static volatile sig_atomic_t keep_running = 1;

static void stop_signal_handler(int signal_number) {
    (void)signal_number;
    keep_running = 0;
}

static void print_usage(const char *program) {
    printf("Usage: %s [--port PORT] [--database PATH] [--frontend PATH] [--seed-demo]\n", program);
    printf("\n");
    printf("  --port PORT        HTTP port (default: 8080)\n");
    printf("  --database PATH    SQLite database path (default: backend/data/courier.db)\n");
    printf("  --frontend PATH    Frontend folder (default: frontend)\n");
    printf("  --seed-demo        Add demonstration accounts and shipments to an empty database.\n");
    printf("                       Requires COURIER_DEMO_PASSWORD environment variable.\n");
}

int main(int argc, char **argv) {
    const char *port = "8080";
    const char *database_path = "backend/data/courier.db";
    const char *frontend_path = "frontend";
    const char *demo_password = NULL;
    bool seed_demo = false;
    AppContext application;
    char error[512] = {0};
    int i;
    memset(&application, 0, sizeof(application));
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) { print_usage(argv[0]); return EXIT_SUCCESS; }
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) { port = argv[++i]; continue; }
        if (strcmp(argv[i], "--database") == 0 && i + 1 < argc) { database_path = argv[++i]; continue; }
        if (strcmp(argv[i], "--frontend") == 0 && i + 1 < argc) { frontend_path = argv[++i]; continue; }
        if (strcmp(argv[i], "--seed-demo") == 0) { seed_demo = true; continue; }
        fprintf(stderr, "Unknown or incomplete argument: %s\n", argv[i]);
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (!db_open(&application.database, database_path, error, sizeof(error))) {
        fprintf(stderr, "Database startup error: %s\n", error);
        return EXIT_FAILURE;
    }
    if (!db_initialize_schema(&application.database, error, sizeof(error))) {
        fprintf(stderr, "Database schema error: %s\n", error);
        db_close(&application.database);
        return EXIT_FAILURE;
    }
    if (seed_demo) {
        demo_password = getenv("COURIER_DEMO_PASSWORD");
        if (demo_password == NULL || demo_password[0] == '\0') {
            fprintf(stderr, "For security, --seed-demo requires COURIER_DEMO_PASSWORD (minimum 8 chars with upper, lower, and digit).\n");
            db_close(&application.database);
            return EXIT_FAILURE;
        }
        if (!db_seed_demo(&application.database, demo_password, error, sizeof(error))) {
            fprintf(stderr, "Demo data error: %s\n", error);
            db_close(&application.database);
            return EXIT_FAILURE;
        }
        printf("Demo data seeded successfully. Change the demonstration password before any non-local use.\n");
    }
    signal(SIGINT, stop_signal_handler);
    signal(SIGTERM, stop_signal_handler);
    if (!server_start(&application, frontend_path, port)) {
        fprintf(stderr, "Unable to start CivetWeb server on port %s. Check that the frontend directory exists and the port is free.\n", port);
        db_close(&application.database);
        return EXIT_FAILURE;
    }
    printf("Courier Dispatch & Tracking System is running at http://localhost:%s/\n", port);
    printf("Press Ctrl+C to stop the server.\n");
    while (keep_running) {
#if defined(_WIN32)
        Sleep(250);
#else
        sleep(1);
#endif
    }
    printf("\nStopping server...\n");
    server_stop(&application);
    db_close(&application.database);
    return EXIT_SUCCESS;
}
