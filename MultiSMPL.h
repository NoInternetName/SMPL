//
// Created by Nikolay on 17.12.2019.
//

#ifndef PROJECT_MULTISMPL_H
#define PROJECT_MULTISMPL_H

#include <ostream>
#include <iostream>
#include <map>
#include <cmath>

#include "smpl.h"

namespace multiSMPL {

    enum SystemEvents {
        SystemEventMonitor = 1000000000LL,
        SystemEventEnd
    };

    struct Meta {
        std::vector<std::string> devices;
        std::vector<std::string> queues;
    };

    class MultiSMPL {
    private:
        struct QueueInformation {
            std::vector<double> avgWaitTime;
            std::vector<double> avgLength;
            std::vector<double> length;
        };

        struct DeviceInformation {
            std::vector<double> avgReserveTime;
            std::vector<double> avgPercentTime;
        };

        class DecPoint : public std::numpunct<char>
        {
        public:
            DecPoint ( char ch_ ) : ch(ch_) {}
            char do_decimal_point() const { return ch ; }
        private:
            char ch ;
        };

        std::ostream *fileOutputStream, *csvOutputStream;
        std::map<smpl::u64, void (*)(std::pair<smpl::u64, smpl::transact_t>, int, smpl::Engine *)> handlers;
        Meta meta;

        void updateDevicesInformation(std::vector<DeviceInformation> &devicesInformation,
                std::vector<smpl::Device *> &devices, time_t time, int number);
        void updateQueuesInformation(std::vector<QueueInformation> &queuesInformation,
                std::vector<smpl::Queue *> &queues, time_t time,  int number);

        void updateDeviceInformation(DeviceInformation &deviceInformation, smpl::Device *device, time_t time,
                int number);
        void updateQueueInformation(QueueInformation &queueInformation, smpl::Queue * queue, time_t time, int number);

        static double avgSum(const std::vector<double> &values, int l, int r);
    public:
        MultiSMPL(const std::map<smpl::u64, void (*)(std::pair<smpl::u64, smpl::transact_t>, int, smpl::Engine *)> &handlers,
                Meta &DevicesAndQueuesNames, std::ostream *fileOutputStream, std::ostream *csvOutputStream);

        void run(int testsCount, std::pair<smpl::u64, smpl::transact_t> startEvent, time_t startTime,
                 time_t (*monitorTime)(smpl::transact_t));

        static std::string printCSVTable(const std::vector<std::vector<std::string>> &table);
        template<typename T>
        static std::string toCSVString(T x);

        template <typename T>
        static void addToVector(std::vector<T> &vector, T value, int index);

        template <typename T>
        static std::vector<std::vector<T>> makeTable(const std::vector<T> &value, T step);

        template <typename T>
        static std::vector<std::vector<T>> makeTable(const std::vector<T> &first, const std::vector<T> &second);

        template <typename T>
        static std::vector<std::vector<std::string>> toStringTable(const std::vector<std::vector<T>> &table,
                std::string (*toString)(T));

        template <typename T>
        static std::vector<std::string> toStringVector(const std::vector<T> &vector, std::string (*toString)(T));

        template <typename T>
        static std::vector<T> division(const std::vector<T> &vec, T value);

        static std::vector<double> welch(const std::vector<double> &values, int w);
    };

    MultiSMPL::MultiSMPL(
            const std::map<smpl::u64, void (*)(std::pair<smpl::u64, smpl::transact_t>, int, smpl::Engine *)> &handlers,
            Meta &DevicesAndQueuesNames, std::ostream *fileOutputStream = nullptr, std::ostream *csvOutputStream = nullptr) {
        this->handlers = handlers;
        this->fileOutputStream = fileOutputStream;
        this->csvOutputStream = csvOutputStream;
        this->meta = DevicesAndQueuesNames;
    }

    void MultiSMPL::run(int testsCount, std::pair<smpl::u64, smpl::transact_t> startEvent, time_t startTime,
            time_t (*monitorTime)(smpl::transact_t)) {
        assert(startTime >= 0);

        std::vector<QueueInformation> queuesInformation((int)meta.queues.size());
        std::vector<DeviceInformation> devicesInformation((int)meta.devices.size());
        std::vector<double> monitoringTimes;

        for (int i = 0; i < testsCount; i++) {
            if (fileOutputStream != nullptr)
                *fileOutputStream << std::endl << "Прогон номер " << i + 1 << std::endl << std::endl;
            smpl::Engine * e = new smpl::Engine(this->fileOutputStream);

            for (int i = 0; i < meta.queues.size(); i++) {
                e->createQueue(meta.queues[i]);
            }

            for (int i = 0; i < meta.devices.size(); i++) {
                e->createDevice(meta.devices[i]);
            }

            e->schedule(startEvent.first, startTime, startEvent.second);
            if (monitorTime != nullptr)
                e->schedule(SystemEventMonitor, monitorTime(0), 0);

            int event = SystemEventEnd;

            do {

                std::pair<smpl::u64, smpl::transact_t> top = e->cause();

                event = top.first;
                smpl::transact_t transact = top.second;

                if (event == SystemEventMonitor) {

                    updateDevicesInformation(devicesInformation, e->getDevices(), e->getTime(), (int)transact);
                    updateQueuesInformation(queuesInformation, e->getQueues(), e->getTime(), (int)transact);

                    if (monitoringTimes.size() <= (int)transact)
                        monitoringTimes.push_back(e->getTime());
                    e->schedule(SystemEventMonitor, monitorTime(transact + 1), transact + 1);
                }
                handlers[event](top, i, e);

            } while (event != SystemEventEnd);

            if (fileOutputStream != nullptr) e->monitor();
            if (fileOutputStream != nullptr) e->report();

            if (fileOutputStream != nullptr) {
                for (int i = 0; i < 1000; i++)
                    *fileOutputStream << "-";
                *fileOutputStream << std::endl;
            }

            delete e;
        }

        for (int i = 0; i < meta.devices.size(); i++) {
            if (fileOutputStream != nullptr)
                *fileOutputStream << "Усредненные срезы параметров устройства " << meta.devices[i] << std::endl;
            if (csvOutputStream != nullptr)
                *csvOutputStream << "Усредненные срезы параметров устройства " << meta.devices[i] << std::endl;

            if (fileOutputStream != nullptr) {
                *fileOutputStream << "Среднее время работы: " << std::endl;
                *fileOutputStream << smpl::Engine::printTable(toStringTable(
                        makeTable(monitoringTimes, division(devicesInformation[i].avgReserveTime,(double)testsCount) ),
                        smpl::Engine::toString)) << std::endl;
            }

            if (csvOutputStream != nullptr) {
                *csvOutputStream << "Среднее время работы: " << std::endl;
                *csvOutputStream << printCSVTable(toStringTable(
                        makeTable(monitoringTimes, division(devicesInformation[i].avgReserveTime, (double)testsCount)),
                        toCSVString)) << std::endl;
            }

            if (fileOutputStream != nullptr) {
                *fileOutputStream << "Средний процент работы: " << std::endl;
                *fileOutputStream << smpl::Engine::printTable(toStringTable(
                        makeTable(monitoringTimes, division(devicesInformation[i].avgPercentTime, (double)testsCount)),
                        smpl::Engine::toString)) << std::endl;
            }

            if (csvOutputStream != nullptr) {
                *csvOutputStream << "Средний процент работы: " << std::endl;
                *csvOutputStream << printCSVTable(toStringTable(
                        makeTable(monitoringTimes, division(devicesInformation[i].avgPercentTime, (double)testsCount)),
                        toCSVString)) << std::endl;
            }
        }

        for (int i = 0; i < meta.queues.size(); i++) {
            if (fileOutputStream != nullptr)
                *fileOutputStream << "Усредненные срезы параметров очереди " << meta.queues[i] << std::endl;
            if (csvOutputStream != nullptr)
                *csvOutputStream << "Усредненные срезы параметров очереди " << meta.queues[i] << std::endl;

            if (fileOutputStream != nullptr) {
                *fileOutputStream << "Средняя длина очереди: " << std::endl;
                *fileOutputStream << smpl::Engine::printTable(toStringTable(
                        makeTable(monitoringTimes, division(queuesInformation[i].avgLength, (double)testsCount)),
                        smpl::Engine::toString)) << std::endl;
            }

            if (csvOutputStream != nullptr) {
                *csvOutputStream << "Средняя длина очереди: " << std::endl;
                *csvOutputStream << printCSVTable(
                        toStringTable(
                                makeTable(monitoringTimes, division(queuesInformation[i].avgLength, (double)testsCount)),
                                toCSVString)) << std::endl;
            }

            if (fileOutputStream != nullptr) {
                *fileOutputStream << "Время ожидания: " << std::endl;
                *fileOutputStream << smpl::Engine::printTable(toStringTable(
                        makeTable(monitoringTimes, division(queuesInformation[i].avgWaitTime, (double)testsCount)),
                        smpl::Engine::toString)) << std::endl;
            }

            if (csvOutputStream != nullptr) {
                *csvOutputStream << "Время ожидания: " << std::endl;
                *csvOutputStream << printCSVTable(
                        toStringTable(makeTable(
                                monitoringTimes,division(queuesInformation[i].avgWaitTime, (double)testsCount)),
                                        toCSVString)) << std::endl;
            }

            if (fileOutputStream != nullptr) {
                *fileOutputStream << "Длина очереди: " << std::endl;
                *fileOutputStream << smpl::Engine::printTable(toStringTable(
                        makeTable(monitoringTimes, division(queuesInformation[i].length, (double)testsCount)),
                        smpl::Engine::toString)) << std::endl;
            }

            if (csvOutputStream != nullptr) {
                *csvOutputStream << "Длина очереди: " << std::endl;
                *csvOutputStream << printCSVTable(
                        toStringTable(makeTable(monitoringTimes, division(queuesInformation[i].length,
                                (double)testsCount)), toCSVString)) << std::endl;
            }
        }
    }

    std::string MultiSMPL::printCSVTable(const std::vector<std::vector<std::string>> &table) {
        std::string result = "";
        for (int i = 0; i < table.size(); i++) {
            for (int j = 0; j < table[i].size(); j++) {
                result += table[i][j] + ";";
            }
            result += "\n";
        }
        return result;
    }

    template<typename T>
    std::string MultiSMPL::toCSVString(T x) {
        std::stringstream ss;
        ss.imbue(std::locale(ss.getloc(), new DecPoint(',')));
        ss << std::fixed << std::setprecision(5) << x;
        return ss.str();
    }

    void MultiSMPL::updateDevicesInformation(std::vector<DeviceInformation> &devicesInformation,
                                             std::vector<smpl::Device *> &devices, time_t time, int number) {
        for (int i = 0; i < devices.size(); i++) {
            updateDeviceInformation(devicesInformation[i], devices[i], time, number);
        }
    }

    void MultiSMPL::updateQueuesInformation(std::vector<QueueInformation> &queuesInformation,
                                            std::vector<smpl::Queue *> &queues, time_t time, int number) {
        for (int i = 0; i < queues.size(); i++) {
            updateQueueInformation(queuesInformation[i], queues[i], time, number);
        }
    }

    void MultiSMPL::updateDeviceInformation(MultiSMPL::DeviceInformation &deviceInformation, smpl::Device *device,
                                            time_t time, int number) {
        addToVector(deviceInformation.avgReserveTime, device->transactCount?
                                                      device->timeUsedSum * 1.0 / device->transactCount:0, number);
        addToVector(deviceInformation.avgPercentTime, device->timeUsedSum * 1.0 / time * 100, number);
    }

    void
    MultiSMPL::updateQueueInformation(MultiSMPL::QueueInformation &queueInformation, smpl::Queue *queue, time_t time,
                                      int number) {
        addToVector(queueInformation.avgLength, queue->timeQueueSum*1.0 / time, number);
        addToVector(queueInformation.avgWaitTime, queue->count?
                                                  queue->waitTimeSum * 1.0 / queue->count:0, number);
        addToVector(queueInformation.length, (double)queue->length(), number);
    }

    template <typename T>
    void MultiSMPL::addToVector(std::vector<T> &vector, T value, int index) {
        assert(index >= 0);
        while (vector.size() <= index)
            vector.push_back(0.0);
        vector[index] += value;
    }

    template <typename T>
    std::vector<T> MultiSMPL::division(const std::vector<T> &vec, T value) {
        std::vector<double> result = vec;
        for (int i = 0; i < vec.size(); i++) {
            result[i] /= value;
        }
        return result;
    }

    template <typename T>
    std::vector<std::vector<T>> MultiSMPL::makeTable(const std::vector<T> &value, T step) {
        std::vector<std::vector<T>> result(2, std::vector<T>(value.size()));
        for (int i = 0; i < value.size(); i++) {
            result[0][i] = (i + 1) * step;
            result[1][i] = value[i];
        }
        return result;
    }

    template <typename T>
    std::vector<std::vector<std::string>>
    MultiSMPL::toStringTable(const std::vector<std::vector<T>> &table, std::string (*toString)(T)) {
        if (table.size() == 0) return std::vector<std::vector<std::string>>(0);
        std::vector<std::vector<std::string>> result(table.size());
        for (int i = 0; i < table.size(); i++) {
            result[i] = toStringVector(table[i], toString);
        }
        return result;
    }

    template<typename T>
    std::vector<std::string> MultiSMPL::toStringVector(const std::vector<T> &vector, std::string (*toString)(T)) {
        std::vector<std::string> result(vector.size());
        for (int i = 0; i < vector.size(); i++) {
            result[i] = !__isnan(vector[i]) ? toString(vector[i]) : "-";
        }
        return result;
    }

    template<typename T>
    std::vector<std::vector<T>> MultiSMPL::makeTable(const std::vector<T> &first, const std::vector<T> &second) {
        if (first.size() != second.size()) return std::vector<std::vector<T>>(0);
        std::vector<std::vector<T>> result(2, std::vector<T>(first.size()));
        for (int i = 0; i < first.size(); i++) {
            result[0][i] = first[i];
            result[1][i] = second[i];
        }
        return result;
    }

    std::vector<double> MultiSMPL::welch(const std::vector<double> &values, int w) {
        std::vector<double> result(values.size() - w);
        for (int i = 0; i < (int)values.size() - w; i++) {
            result[i] = avgSum(values, i - std::min(w, i), i + std::min(w, i));
        }
        return result;
    }

    double MultiSMPL::avgSum(const std::vector<double> &values, int l, int r) {
        double res = 0;
        for (int i = l; i <= r; i++) {
            res += values[i];
        }
        return res / (r - l + 1);
    }
}

#endif //PROJECT_MULTISMPL_H
