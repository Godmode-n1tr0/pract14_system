#include <windows.h>
#include <iostream>
#include <ctime>
#include <string>

using namespace std;

#define MAX_CLIENTS 20
#define CLUB_CAPACITY 4

struct ClientRecord {
    DWORD threadId;
    DWORD arriveTick;
    DWORD startTick;
    DWORD endTick;
    BOOL served;
    BOOL timeout;
};

struct ClubState {
    ClientRecord clients[MAX_CLIENTS];
    LONG currentVisitors;
    LONG maxVisitors;
    LONG servedCount;
    LONG timeoutCount;
};

ClubState club;
HANDLE hSemaphore;
CRITICAL_SECTION logCS;

void log(string text) {
    EnterCriticalSection(&logCS);
    cout << text << endl;
    LeaveCriticalSection(&logCS);
}

DWORD WINAPI ClientThread(LPVOID param) {
    int id = (int)param;

    club.clients[id].threadId = GetCurrentThreadId();
    club.clients[id].arriveTick = GetTickCount();

    log("Клиент " + to_string(id) + " пришел");

    DWORD waitResult = WaitForSingleObject(hSemaphore, 3000);

    if (waitResult == WAIT_OBJECT_0) {
        log("Клиент " + to_string(id) + " занял место");

        InterlockedIncrement(&club.currentVisitors);

        if (club.currentVisitors >= club.maxVisitors)
            club.maxVisitors = club.currentVisitors;

        club.clients[id].startTick = GetTickCount();

        int workTime = 2000 + rand() % 3000;
        Sleep(workTime);

        club.clients[id].endTick = GetTickCount();
        club.clients[id].served = TRUE;

        log("Клиент " + to_string(id) + " освободил место");

        InterlockedDecrement(&club.currentVisitors);
        InterlockedIncrement(&club.servedCount);

        ReleaseSemaphore(hSemaphore, 1, NULL);
    }
    else {
        club.clients[id].timeout = TRUE;
        InterlockedIncrement(&club.timeoutCount);

        log("Клиент " + to_string(id) + " ушел (не дождался)");
    }

    return 0;
}

DWORD WINAPI ObserverThread(LPVOID param) {
    while (true) {
        Sleep(500);

        log("Состояние -> занято мест - " + to_string(club.currentVisitors) +
            " | обслужено - " + to_string(club.servedCount) +
            " | ушли - " + to_string(club.timeoutCount));

        if (club.servedCount + club.timeoutCount == MAX_CLIENTS)
            break;
    }
    return 0;
}

void printStats() {
    double totalWait = 0;
    double totalService = 0;
    int served = 0;

    cout << "\n---- СТАТИСТИКА ----" << endl;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (club.clients[i].served) {
            totalWait += (club.clients[i].startTick - club.clients[i].arriveTick);
            totalService += (club.clients[i].endTick - club.clients[i].startTick);
            served++;
        }
    }

    if (served > 0) {
        cout << "Среднее ожидание - " << totalWait / served << " мс" << endl;
        cout << "Среднее обслуживание - " << totalService / served << " мс" << endl;
    }

    cout << "Максимум занятых мест - " << club.maxVisitors << endl;

    cout << "Не дождались - ";
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (club.clients[i].timeout)
            cout << i << " ";
    }
    cout << endl;
}

int main() {
    setlocale(LC_ALL, "ru");
    srand(time(0));

    ZeroMemory(&club, sizeof(club));
    InitializeCriticalSection(&logCS);

    hSemaphore = CreateSemaphore(NULL, CLUB_CAPACITY, CLUB_CAPACITY, NULL);

    HANDLE threads[MAX_CLIENTS];

    for (int i = 0; i < MAX_CLIENTS; i++) {
        threads[i] = CreateThread(NULL, 0, ClientThread, (LPVOID)i, 0, NULL);

        if (i < 8)
            SetThreadPriority(threads[i], THREAD_PRIORITY_NORMAL);
        else if (i < 16)
            SetThreadPriority(threads[i], THREAD_PRIORITY_BELOW_NORMAL);
        else
            SetThreadPriority(threads[i], THREAD_PRIORITY_HIGHEST);
    }

    HANDLE observer = CreateThread(NULL, 0, ObserverThread, NULL, 0, NULL);

    WaitForMultipleObjects(MAX_CLIENTS, threads, TRUE, INFINITE);
    WaitForSingleObject(observer, INFINITE);

    printStats();

    DeleteCriticalSection(&logCS);
    CloseHandle(hSemaphore);

    return 0;
}