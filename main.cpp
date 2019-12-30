#include <iostream>
#include <fstream>
#include <vector>
#include "MultiSMPL.h"
#include "smpl.h"

using namespace std;
using namespace multiSMPL;
using namespace smpl;

const int SCALE = 100;
const int MONTH = 120;
const int GENERATE_TIME = 0.1 * SCALE;
const double END_TIME = MONTH * SCALE;
const int LEVEL = 10;
const int MAX_SIZE = 50;
const int MIN_TIME_ORDER = 0.5 * SCALE;
const int MAX_TIME_ORDER = SCALE;
const int K = 30;
const int N = 3;
const int P = 5;
const int H = 1;
const int MONITOR = 5;

int testCount = 20;

vector<double> C = {1, 1, 1, 7, 10, 30, 30, 20, 20};
vector<vector<double>> values(testCount, vector<double>(9));

time_t last;

double I, IPos, INeg, IPosAcc, IAcc, INegAcc;
double money = 0;

enum events {
    EventStart = 1, // Начало моделирования
    EventGenerate, // Приход заявки
    EventGetOrder, // Приход заказа
    EventCheck // Проверка состояния склада
};

// функция для отображения номера (начиная с 0) среза на время, которое пройдет с момента предыдущего аналогичного события
// функция должна быть определена для любого transact >= 0
time_t deltaTime(transact_t transact) {
    //return SCALE * MONITOR;
    if (transact < C.size())
        return C[transact] * SCALE;
    else return 1e9;
}

int count(double value) {
    if (value <= 0.1)
        return 1;
    else if (value <= 0.4)
        return 2;
    else if (value <= 0.8)
        return 3;
    else return 4;
}

/*
 * Обработчик события "Начало моделирования" Определяем все нужные начальные события, а также обнуляем переменные,
 * которые используются в обработчиках и валидны в течении одного прогона.
*/
void EventStartHandler(pair<u64, transact_t> event, int testNumber, Engine * e) {
    last = 0;
    I = IPos = INeg = IPosAcc = IAcc = INegAcc = 0;
    I = IPos = 50;
    money = 0;
    e->schedule(EventGenerate, e->negExp(GENERATE_TIME), 1);
    e->schedule(EventCheck, 0, 1);
    e->schedule(SystemEventEnd, MONTH * SCALE,1);
}

// Обработчик события "Приход заявки"
void EventGenerateHandler(pair<u64, transact_t> event, int testNumber, Engine * e) {
    int cnt = count(e->fRandom());
    IPosAcc += IPos * (int)(e->getTime() - last);
    INegAcc += INeg * (int)(e->getTime() - last);
    I -= cnt;
    IPos = max(0.0, I);
    INeg = max(0.0, -I);
    last = e->getTime();
    int t = e->negExp(GENERATE_TIME);
    e->schedule(EventGenerate, t, event.second + 1);
//    cerr << "Pay " << I << ' ' << IPosAcc / e->getTime() << ' ' << INegAcc / e->getTime() << ' ' << e->getTime() * 1. / SCALE << endl;
}

// Обработчик события "Поступление заказа"
void EventGetOrderHandler(pair<u64, transact_t> event, int testNumber, Engine * e) {
    money += K + N * event.second;
    IPosAcc += IPos * (int)(e->getTime() - last);
    INegAcc += INeg * (int)(e->getTime() - last);
    I += event.second;
    IPos = max(0.0, I);
    INeg = max(0.0, -I);
    last = e->getTime();
//    cerr << "Get " << I << ' ' << IPosAcc / e->getTime() << ' ' << INegAcc / e->getTime() << ' ' << e->getTime() * 1. / SCALE << endl;
}

// Обработчик события "Проверка состояния склада"
void EventCheckHandler(pair<u64, transact_t> event, int testNumber, Engine * e) {
    if (I < LEVEL) {
        e->schedule(EventGetOrder, e->iRandom(MIN_TIME_ORDER, MAX_TIME_ORDER), MAX_SIZE - I);
//        cerr << "Order " << I << ' ' << e->getTime() * 1. / SCALE << endl;
    }
    e->schedule(EventCheck, SCALE, 1);
}

// Обработчик события "Монитор для срезов" определен в модуле MultiSMPL.h обязателен
void EventMonitorHandler(pair<u64, transact_t> event, int testNumber, Engine * e) {
    values[testNumber][event.second] = money * SCALE / e->getTime() + IPosAcc / e->getTime() * H + INegAcc / e->getTime() * P;
}

// Обработчик события "Окончание моделирования" определен в модуле MultiSMPL.h обязателен
void EventEndHandler(pair<u64, transact_t> event, int testNumber, Engine * e) {

}

int main() {
    srand(456987);
    map<u64, void(*)(pair<u64, transact_t >, int, Engine *)> handlers;  // Задает соответствие Событие - функция,
                                                                        // которая его обрабатывает
    handlers[EventStart] = EventStartHandler;
    handlers[EventGenerate] = EventGenerateHandler;
    handlers[EventGetOrder] = EventGetOrderHandler;
    handlers[EventCheck] = EventCheckHandler;
    handlers[SystemEventEnd] = EventEndHandler;
    handlers[SystemEventMonitor] = EventMonitorHandler;
    Meta buf = {}; // Структура с именами устройств и очередей соответственно
    ofstream out("test.txt"); // Вывод в текстовый файл
    ofstream csvOut("test.csv"); // Вывод в файл csv (для открытия в Exeд, Open office, Libre Office) структура файла
                                    // (разделитель ";", целая часть от дробной отделена ",", Кодировка UTF-8)
    MultiSMPL * test = new MultiSMPL(handlers, buf, nullptr, nullptr);
    test->run(testCount, {EventStart, 0}, 0, deltaTime); // Передаем количество прогонов,
                                                                                   // начальное событие, время,
                                                                                   // через которое оно произойдет,
                                                                                   // а также функцию, которая отображает
                                                                                   // номер вызова монитора на время,
                                                                                   // через которое оно произойдет
    cout << money / MONTH << ' ' << (double)I << ' ' << (double)IPosAcc / END_TIME << ' ' << (double)INegAcc / END_TIME << ' '
         << money / MONTH + IPosAcc / END_TIME * H + INegAcc / END_TIME * P << endl;
    vector<double> avg(values[0].size());
    for (auto i : values) {
        csvOut << MultiSMPL::printCSVTable({MultiSMPL::toStringVector(i, MultiSMPL::toCSVString)});
        for (int j = 0; j < i.size(); j++) {
            avg[j] += i[j];
        }
    }
    csvOut << endl << endl << endl << MultiSMPL::printCSVTable({MultiSMPL::toStringVector(MultiSMPL::welch(MultiSMPL::division(avg, (double)testCount), 5), MultiSMPL::toCSVString)});
    // в модуле есть полезные функции по форматированию данных для вывода в файл (например, Exel для их дальнейшей
    // обработки)
    return 0;
}
