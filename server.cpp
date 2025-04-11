#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <windows.h>
#include <iostream>
#include <thread>
#include <string>
#include <mysql/jdbc.h>
#pragma comment(lib, "ws2_32.lib")

using namespace std;

const int PORT = 12345;
const int BUFFER_SIZE = 1024;

sql::mysql::MySQL_Driver* driver;
shared_ptr<sql::Connection> dbConn;


string processMessage(const string& msg, int& userId, int& sessionId) {
    if (msg.find("REGISTER:") == 0) {
        string body = msg.substr(9);
        size_t idEnd = body.find(":");
        size_t pwEnd = body.rfind(":");
        if (idEnd == string::npos || pwEnd == string::npos || idEnd == pwEnd) return "Invalid format";

        string id = body.substr(0, idEnd);
        string pw = body.substr(idEnd + 1, pwEnd - idEnd - 1);

        try {
            unique_ptr<sql::PreparedStatement> checkStmt(
                dbConn->prepareStatement("SELECT * FROM users WHERE username = ?")
            );
            checkStmt->setString(1, id);
            unique_ptr<sql::ResultSet> res(checkStmt->executeQuery());
            if (res->next()) return "ID already exists";

            unique_ptr<sql::PreparedStatement> insertStmt(
                dbConn->prepareStatement("INSERT INTO users (username, password) VALUES (?, ?)")
            );
            insertStmt->setString(1, id);
            insertStmt->setString(2, pw);
            insertStmt->executeUpdate();
            return "Register Success";
        }
        catch (...) {
            return "Register Failed";
        }
    }
    else if (msg.find("LOGIN:") == 0) {
        string body = msg.substr(6);
        size_t idEnd = body.find(":");
        size_t pwEnd = body.rfind(":");
        if (idEnd == string::npos || pwEnd == string::npos || idEnd == pwEnd) return "Invalid format";

        string id = body.substr(0, idEnd);
        string pw = body.substr(idEnd + 1, pwEnd - idEnd - 1);

        try {
            unique_ptr<sql::PreparedStatement> stmt(
                dbConn->prepareStatement("SELECT user_id FROM users WHERE username=? AND password=?")
            );
            stmt->setString(1, id);
            stmt->setString(2, pw);
            unique_ptr<sql::ResultSet> res(stmt->executeQuery());

            if (res->next()) {
                userId = res->getInt("user_id");
                unique_ptr<sql::PreparedStatement> sessionStmt(
                    dbConn->prepareStatement("INSERT INTO user_sessions (user_id, ip_address, logout_time) VALUES (?, ?, NULL)")
                );
                sessionStmt->setInt(1, userId);
                sessionStmt->setString(2, "127.0.0.1");
                sessionStmt->executeUpdate();

                // 세션 ID 조회
                unique_ptr<sql::PreparedStatement> idStmt(
                    dbConn->prepareStatement("SELECT LAST_INSERT_ID() AS session_id")
                );
                unique_ptr<sql::ResultSet> idRes(idStmt->executeQuery());
                if (idRes->next()) sessionId = idRes->getInt("session_id");

                return "Login Success";
            }
            else {
                return "Login Failed";
            }
        }
        catch (...) {
            return "Login Failed";
        }
    }
    else if (msg.find("CHAT:") == 0) {
        string content = msg.substr(5);
        if (userId == -1) return "Not logged in";

        try {
            unique_ptr<sql::PreparedStatement> stmt(
                dbConn->prepareStatement("INSERT INTO message_log (sender_id, content) VALUES (?, ?)")
            );
            stmt->setInt(1, userId);
            stmt->setString(2, content);
            stmt->executeUpdate();
            return "Echo: " + content;
        }
        catch (...) {
            return "Chat Save Failed";
        }
    }
    else if (msg == "exit") {
        // 로그아웃 시간 기록
        if (sessionId != -1) {
            try {
                unique_ptr<sql::PreparedStatement> logoutStmt(
                    dbConn->prepareStatement("UPDATE user_sessions SET logout_time = CURRENT_TIMESTAMP WHERE session_id = ?")
                );
                logoutStmt->setInt(1, sessionId);
                logoutStmt->executeUpdate();
            }
            catch (...) {
                // 무시
            }
        }
        return "Goodbye";
    }
    return "Invalid Command";
}

void handleClient(SOCKET clientSocket) {
    int userId = -1; // 현재 접속한 클라이언트의 사용자 ID
    int sessionId = -1; // 현재 세션 ID
    char buffer[BUFFER_SIZE];
    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int len = recv(clientSocket, buffer, BUFFER_SIZE, 0);
        if (len <= 0) break;
        string msg(buffer);
        cout << u8"[클라이언트 메시지] " << msg << endl;

        string response = processMessage(msg, userId, sessionId);
        send(clientSocket, response.c_str(), response.size(), 0);

        if (msg == "exit") break;
    }
    closesocket(clientSocket);
    cout << u8"클라이언트 연결 종료\n";
}

int main() {
    SetConsoleCP(65001);
    SetConsoleOutputCP(65001);

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << u8"WSAStartup 실패" << endl;
        return 1;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        cerr << u8"소켓 생성 실패" << endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    if (::bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << u8"바인딩 실패" << endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        cerr << u8"리스닝 실패" << endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    cout << u8"서버 실행 중... 포트 " << PORT << endl;

    try {
        driver = sql::mysql::get_mysql_driver_instance();
        dbConn.reset(driver->connect("tcp://127.0.0.1:3306", "root", "7606"));
        dbConn->setSchema("chat_management");

        unique_ptr<sql::Statement> charsetStmt(dbConn->createStatement());
        charsetStmt->execute("SET NAMES utf8mb4");
    }
    catch (sql::SQLException& e) {
        cerr << u8"DB 연결 실패: " << e.what() << endl;
        return 1;
    }

    while (true) {
        sockaddr_in clientAddr;
        int clientSize = sizeof(clientAddr);
        SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientSize);
        if (clientSocket == INVALID_SOCKET) {
            cerr << u8"클라이언트 연결 실패" << endl;
            continue;
        }
        cout << u8"[클라이언트 접속됨]" << endl;
        thread(handleClient, clientSocket).detach();
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}
