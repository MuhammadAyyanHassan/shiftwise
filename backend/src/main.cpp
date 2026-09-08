#include <crow.h>
#include <cstdlib>
#include <iostream>
#include <string>
using namespace std;

// Include all controller headers (NOT .cpp files)
#include "controllers/AuthController.h"
#include "controllers/StaffController.h"
#include "controllers/LeaveController.h"
#include "controllers/ScheduleController.h"
#include "controllers/DashboardController.h"

// Include middleware
#include "middleware/AuthMiddleware.h"

// ─────────────────────────────────────────────
//  main  —  starts the Crow HTTP server and
//  registers every API route with proper auth
// ─────────────────────────────────────────────
int main () {
    // ── Read environment variables ────────────
    // These are set in Render.com's dashboard (never hardcode secrets!)
    string dbUrl    = getenv("DATABASE_URL") ? getenv("DATABASE_URL") : "";
    string jwtSecret = getenv("JWT_SECRET")  ? getenv("JWT_SECRET")  : "";
    string portStr  = getenv("PORT")         ? getenv("PORT")        : "8080";
    int    port     = stoi(portStr);

    // Validate required environment variables
    if (dbUrl.empty()) {
        cerr << "ERROR: DATABASE_URL environment variable is not set." << endl;
        return 1;
    }
    if (jwtSecret.empty()) {
        cerr << "ERROR: JWT_SECRET environment variable is not set." << endl;
        return 1;
    }

    cout << "Starting ShiftWise backend on port " << port << endl;

    // ── Create controller instances ───────────
    // Each controller handles a group of related routes.
    // We pass the DB connection string and JWT secret to every one.
    AuthController     authCtrl    (dbUrl, jwtSecret);
    StaffController    staffCtrl   (dbUrl, jwtSecret);
    LeaveController    leaveCtrl   (dbUrl, jwtSecret);
    ScheduleController schedCtrl   (dbUrl, jwtSecret);
    DashboardController dashCtrl   (dbUrl, jwtSecret);
    
    // ── Create auth middleware instance ───────
    AuthMiddleware authMiddleware(jwtSecret);

    // ── Set up the web application ────────────
    crow::SimpleApp app;

    // CORS middleware — needed so the React frontend can talk to this server
    app.before_handle([](crow::request& req, crow::response& res, crow::App<>::context&) {
        res.add_header("Access-Control-Allow-Origin",  "*");
        res.add_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.add_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        if (req.method == crow::HTTPMethod::Options) {
            res.code = 204;
            res.end();
        }
    });

    // ── Auth routes (PUBLIC) ───────────────────
    CROW_ROUTE(app, "/api/auth/signup").methods("POST"_method)
    ([&authCtrl](const crow::request& req) {
        return authCtrl.signup(req);
    });

    CROW_ROUTE(app, "/api/auth/login").methods("POST"_method)
    ([&authCtrl](const crow::request& req) {
        return authCtrl.login(req);
    });

    CROW_ROUTE(app, "/api/auth/me").methods("GET"_method)
    ([&authCtrl](const crow::request& req) {
        return authCtrl.getMe(req);
    });

    // Admin approval routes (PROTECTED - Manager only)
    CROW_ROUTE(app, "/api/auth/pending-managers").methods("GET"_method)
    ([&authCtrl, &authMiddleware](const crow::request& req) {
        // Verify JWT and manager role
        string token = authMiddleware.extractToken(req);
        if (!authMiddleware.isManager(token)) {
            return crow::response(403, "{\"error\":\"Manager access required\"}");
        }
        return authCtrl.getPendingManagers(req);
    });

    CROW_ROUTE(app, "/api/auth/approve/<string>").methods("PUT"_method)
    ([&authCtrl, &authMiddleware](const crow::request& req, string id) {
        // Verify JWT and manager role
        string token = authMiddleware.extractToken(req);
        if (!authMiddleware.isManager(token)) {
            return crow::response(403, "{\"error\":\"Manager access required\"}");
        }
        return authCtrl.updateManagerStatus(req, id, "active");
    });

    CROW_ROUTE(app, "/api/auth/reject/<string>").methods("PUT"_method)
    ([&authCtrl, &authMiddleware](const crow::request& req, string id) {
        // Verify JWT and manager role
        string token = authMiddleware.extractToken(req);
        if (!authMiddleware.isManager(token)) {
            return crow::response(403, "{\"error\":\"Manager access required\"}");
        }
        return authCtrl.updateManagerStatus(req, id, "rejected");
    });

    // ── Staff routes (PROTECTED - Manager only) ────────────────────
    CROW_ROUTE(app, "/api/staff").methods("GET"_method)
    ([&staffCtrl, &authMiddleware](const crow::request& req) {
        string token = authMiddleware.extractToken(req);
        if (!authMiddleware.isManager(token)) {
            return crow::response(403, "{\"error\":\"Manager access required\"}");
        }
        return staffCtrl.getAllStaff(req);
    });

    CROW_ROUTE(app, "/api/staff/<int>").methods("GET"_method)
    ([&staffCtrl, &authMiddleware](const crow::request& req, int id) {
        string token = authMiddleware.extractToken(req);
        if (!authMiddleware.isManager(token)) {
            return crow::response(403, "{\"error\":\"Manager access required\"}");
        }
        return staffCtrl.getStaffById(req, id);
    });

    CROW_ROUTE(app, "/api/staff").methods("POST"_method)
    ([&staffCtrl, &authMiddleware](const crow::request& req) {
        string token = authMiddleware.extractToken(req);
        if (!authMiddleware.isManager(token)) {
            return crow::response(403, "{\"error\":\"Manager access required\"}");
        }
        return staffCtrl.createStaff(req);
    });

    CROW_ROUTE(app, "/api/staff/<int>").methods("PUT"_method)
    ([&staffCtrl, &authMiddleware](const crow::request& req, int id) {
        string token = authMiddleware.extractToken(req);
        if (!authMiddleware.isManager(token)) {
            return crow::response(403, "{\"error\":\"Manager access required\"}");
        }
        return staffCtrl.updateStaff(req, id);
    });

    CROW_ROUTE(app, "/api/staff/<int>").methods("DELETE"_method)
    ([&staffCtrl, &authMiddleware](const crow::request& req, int id) {
        string token = authMiddleware.extractToken(req);
        if (!authMiddleware.isManager(token)) {
            return crow::response(403, "{\"error\":\"Manager access required\"}");
        }
        return staffCtrl.deleteStaff(req, id);
    });

    // ── Leave routes (PROTECTED - Any authenticated user) ────────────
    CROW_ROUTE(app, "/api/leave").methods("GET"_method)
    ([&leaveCtrl, &authMiddleware](const crow::request& req) {
        if (!authMiddleware.isValidToken(authMiddleware.extractToken(req))) {
            return crow::response(401, "{\"error\":\"Unauthorized\"}");
        }
        return leaveCtrl.getAllLeave(req);
    });

    CROW_ROUTE(app, "/api/leave/my").methods("GET"_method)
    ([&leaveCtrl, &authMiddleware](const crow::request& req) {
        if (!authMiddleware.isValidToken(authMiddleware.extractToken(req))) {
            return crow::response(401, "{\"error\":\"Unauthorized\"}");
        }
        return leaveCtrl.getMyLeave(req);
    });

    CROW_ROUTE(app, "/api/leave").methods("POST"_method)
    ([&leaveCtrl, &authMiddleware](const crow::request& req) {
        if (!authMiddleware.isValidToken(authMiddleware.extractToken(req))) {
            return crow::response(401, "{\"error\":\"Unauthorized\"}");
        }
        return leaveCtrl.submitLeave(req);
    });

    CROW_ROUTE(app, "/api/leave/<int>/approve").methods("PUT"_method)
    ([&leaveCtrl, &authMiddleware](const crow::request& req, int id) {
        string token = authMiddleware.extractToken(req);
        if (!authMiddleware.isManager(token)) {
            return crow::response(403, "{\"error\":\"Manager access required\"}");
        }
        return leaveCtrl.approveLeave(req, id);
    });

    CROW_ROUTE(app, "/api/leave/<int>/reject").methods("PUT"_method)
    ([&leaveCtrl, &authMiddleware](const crow::request& req, int id) {
        string token = authMiddleware.extractToken(req);
        if (!authMiddleware.isManager(token)) {
            return crow::response(403, "{\"error\":\"Manager access required\"}");
        }
        return leaveCtrl.rejectLeave(req, id);
    });

    // ── Schedule routes (PROTECTED) ────────────────────
    CROW_ROUTE(app, "/api/schedule").methods("GET"_method)
    ([&schedCtrl, &authMiddleware](const crow::request& req) {
        if (!authMiddleware.isValidToken(authMiddleware.extractToken(req))) {
            return crow::response(401, "{\"error\":\"Unauthorized\"}");
        }
        return schedCtrl.getAllShifts(req);
    });

    CROW_ROUTE(app, "/api/schedule/my").methods("GET"_method)
    ([&schedCtrl, &authMiddleware](const crow::request& req) {
        if (!authMiddleware.isValidToken(authMiddleware.extractToken(req))) {
            return crow::response(401, "{\"error\":\"Unauthorized\"}");
        }
        return schedCtrl.getMyShifts(req);
    });

    CROW_ROUTE(app, "/api/schedule").methods("POST"_method)
    ([&schedCtrl, &authMiddleware](const crow::request& req) {
        string token = authMiddleware.extractToken(req);
        if (!authMiddleware.isManager(token)) {
            return crow::response(403, "{\"error\":\"Manager access required\"}");
        }
        return schedCtrl.createShift(req);
    });

    CROW_ROUTE(app, "/api/schedule/<int>").methods("PUT"_method)
    ([&schedCtrl, &authMiddleware](const crow::request& req, int id) {
        string token = authMiddleware.extractToken(req);
        if (!authMiddleware.isManager(token)) {
            return crow::response(403, "{\"error\":\"Manager access required\"}");
        }
        return schedCtrl.updateShift(req, id);
    });

    CROW_ROUTE(app, "/api/schedule/<int>").methods("DELETE"_method)
    ([&schedCtrl, &authMiddleware](const crow::request& req, int id) {
        string token = authMiddleware.extractToken(req);
        if (!authMiddleware.isManager(token)) {
            return crow::response(403, "{\"error\":\"Manager access required\"}");
        }
        return schedCtrl.deleteShift(req, id);
    });

    // ── Dashboard routes (PROTECTED) ───────────────
    CROW_ROUTE(app, "/api/dashboard/stats").methods("GET"_method)
    ([&dashCtrl, &authMiddleware](const crow::request& req) {
        if (!authMiddleware.isValidToken(authMiddleware.extractToken(req))) {
            return crow::response(401, "{\"error\":\"Unauthorized\"}");
        }
        return dashCtrl.getManagerStats(req);
    });

    CROW_ROUTE(app, "/api/dashboard/my").methods("GET"_method)
    ([&dashCtrl, &authMiddleware](const crow::request& req) {
        if (!authMiddleware.isValidToken(authMiddleware.extractToken(req))) {
            return crow::response(401, "{\"error\":\"Unauthorized\"}");
        }
        return dashCtrl.getEmployeeStats(req);
    });

    // Health-check route (Render pings this to confirm the server is alive)
    // PUBLIC - no auth needed
    CROW_ROUTE(app, "/health")
    ([]() {
        return crow::response(200, "{\"status\":\"ok\"}");
    });

    // ── Start the server ──────────────────────
    app.port(port)
       .multithreaded()
       .run();

    return 0;
}
