#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <windows.h>
#include <iostream>
#include <string>
#pragma comment(lib, "ws2_32.lib")


using namespace std;

const char* SERVER_IP = "127.0.0.1";
const int PORT = 12345;
const int BUFFER_SIZE = 1024;

void showMenu() {
    cout << "\n======= Chatting System =======\n"
        << u8"1. 회원가입\n"
        << u8"2. 로그인\n"
        << u8"0. 종료\n";
}

void showChatMenu() {
    cout << "\n======= Chat Mode =======\n"
        << u8"1. 채팅하기\n"
        << u8"2. 로그아웃\n"
        << u8"3. 종료\n";
}

// 서버로 메시지 보내고 응답 받기
string sendAndReceive(SOCKET sock, const string& message) {
    send(sock, message.c_str(), message.length(), 0);
    char buffer[BUFFER_SIZE] = { 0 };
    int len = recv(sock, buffer, BUFFER_SIZE, 0);
    if (len <= 0) return u8"(서버 연결 끊김)";
    return string(buffer);
}

int main() {
    SetConsoleCP(65001);          // 입력 인코딩을 UTF-8로
    SetConsoleOutputCP(65001);    // 출력 인코딩을 UTF-8로

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << u8"WSAStartup 실패" << endl;
        return 1;
    }

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        cerr << u8"소켓 생성 실패" << endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(SERVER_IP);
    serverAddr.sin_port = htons(PORT);

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << u8"서버 연결 실패" << endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    cout << u8"서버에 연결되었습니다!" << endl;

    int choice;
    while (true) {
        showMenu();
        cin >> choice;
        string id, pw, response;

        switch (choice) {
        case 1:
            cout << u8"ID 입력: "; cin >> id;
            cout << u8"PW 입력: "; cin >> pw;
            response = sendAndReceive(clientSocket, "REGISTER:" + id + ":" + pw + ":");
            cout << u8"서버 응답: " << response << endl;
            break;
        case 2:
            cout << u8"ID 입력: "; cin >> id;
            cout << u8"PW 입력: "; cin >> pw;
            response = sendAndReceive(clientSocket, "LOGIN:" + id + ":" + pw + ":");
            cout << u8"서버 응답: " << response << endl;
            if (response == "Login Success") {
                int sub;
                cin.ignore();
                while (true) {
                    showChatMenu();
                    cin >> sub;
                    cin.ignore();
                    if (sub == 1) {
                        while (true) {
                            string msg;
                            cout << u8"메시지 입력 (exit 입력 시 채팅 종료): ";
                            getline(cin, msg);
                            if (msg == "exit") break;
                            response = sendAndReceive(clientSocket, "CHAT:" + msg);
                            cout << response << endl;
                        }
                    }
                    else if (sub == 2) {
                        cout << u8"로그아웃 되었습니다." << endl;
                        break;
                    }
                    else if (sub == 3) {
                        send(clientSocket, "exit", 4, 0);
                        closesocket(clientSocket);
                        WSACleanup();
                        return 0;
                    }
                }
            }
            break;
        case 0:
            send(clientSocket, "exit", 4, 0);
            closesocket(clientSocket);
            WSACleanup();
            return 0;
        default:
            cout << u8"잘못된 선택입니다." << endl;
        }
    }

    closesocket(clientSocket);
    WSACleanup();
    return 0;
}
