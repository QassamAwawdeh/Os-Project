#include <iostream>
#include <fstream>
#include <vector>
#include <queue>
#include <algorithm>
#include <climits>
#include <iomanip>
#include <string>

using namespace std;

struct Process {
    int id;
    int arrival_time;
    int burst_time;
    int remaining_time;
    int start_time = -1;
    int finish_time = 0;
    int waiting_time = 0;
    int turnaround_time = 0;
    Process(int id, int arrival, int burst)
        : id(id), arrival_time(arrival), burst_time(burst), remaining_time(burst) {}
};

struct Scheduler {
    vector<Process> processes;
    int context_switch;
    int quantum;
    Scheduler() : context_switch(1), quantum(1) {} // Default values
};

void readInput(const char* filename, Scheduler& scheduler) {
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Failed to open file." << endl;
        exit(1);
    }
    int n;
    file >> n;
    for (int i = 0; i < n; ++i) {
        int id, arrival, burst;
        file >> id >> arrival >> burst;
        scheduler.processes.emplace_back(id, arrival, burst);
    }
    file.close();
}

void printGanttChart(const vector<Process>& processes) {
    cout << "Gantt Chart:" << endl;
    for (const auto& process : processes) {
        cout << "[P" << process.id << " (" << process.start_time << " - " << process.finish_time << ")] ";
    }
    cout << endl;
}

    void calculateMetrics(vector<Process>& processes, Scheduler& scheduler) {
    double total_waiting_time = 0, total_turnaround_time = 0;
    int last_finish_time = 0, total_idle_time = 0, total_context_switch_time = 0;
    int previous_end = 0;

    cout << "\nDetailed Metrics for Each Process:\n";
    cout << left << setw(10) << "Process"
        << setw(15) << "Finish Time"
        << setw(15) << "Waiting Time"
        << setw(20) << "Turnaround Time" << endl;

    for (auto& process : processes) {
        process.turnaround_time = process.finish_time - process.arrival_time;
        process.waiting_time = process.turnaround_time - process.burst_time;
        total_waiting_time += process.waiting_time;
        total_turnaround_time += process.turnaround_time;

        cout << setw(10) << "P" + to_string(process.id)
            << setw(15) << process.finish_time
            << setw(15) << process.waiting_time
            << setw(20) << process.turnaround_time << endl;

        if (process.start_time > previous_end) {
            total_idle_time += process.start_time - previous_end;
        }
        if (&process != &processes.front()) {
            total_context_switch_time += scheduler.context_switch;
        }
        previous_end = process.finish_time;
        last_finish_time = max(last_finish_time, process.finish_time);
    }

    double total_active_time = last_finish_time - processes.front().arrival_time;
    double cpu_utilization = 100.0 * (total_active_time - total_context_switch_time) / total_active_time;

    cout << fixed << setprecision(2);
    cout << "\nAverage Metrics:\n";
    cout << "Average Waiting Time: " << total_waiting_time / processes.size() << " ms" << endl;
    cout << "Average Turnaround Time: " << total_turnaround_time / processes.size() << " ms" << endl;
    cout << "CPU Utilization: " << cpu_utilization << "%" << endl;
}


void fcfs(vector<Process>& processes, int cs, Scheduler& scheduler) {
    sort(processes.begin(), processes.end(), [](const Process& a, const Process& b) {
        return a.arrival_time < b.arrival_time;
        });
    int current_time = 0;
    for (auto& process : processes) {
        if (current_time < process.arrival_time) {
            current_time = process.arrival_time;
        }
        process.start_time = current_time;
        process.finish_time = process.start_time + process.burst_time;
        current_time = process.finish_time;
        if (&process != &processes.back()) {
            current_time += cs;
        }
    }
}

void srt(vector<Process>& processes, int cs, Scheduler& scheduler) {
    auto comp = [](const Process* a, const Process* b) {
        return a->remaining_time > b->remaining_time || (a->remaining_time == b->remaining_time && a->arrival_time > b->arrival_time);
    };
    priority_queue<Process*, vector<Process*>, decltype(comp)> pq(comp);
    int current_time = 0, index = 0;
    Process* last_process = nullptr;

    while (index < processes.size() || !pq.empty()) {
        while (index < processes.size() && processes[index].arrival_time <= current_time) {
            pq.push(&processes[index]);
            if (!pq.empty()) {
                Process* current_process = pq.top();
                // Check if it's worth switching to the new process
                if (current_process != &processes[index] && (processes[index].burst_time + cs) < current_process->remaining_time) {
                    current_time += cs;  // Account for context switch
                    current_process = &processes[index];  // Switch to the new process
                    current_process->start_time = current_time;
                }
            }
            index++;
        }
        if (pq.empty()) {
            if (index < processes.size()) {
                current_time = processes[index].arrival_time;
            }
            continue;
        }

        Process* current_process = pq.top();
        pq.pop();
        if (current_process->start_time == -1) {
            current_process->start_time = current_time;
        }
        
        int next_time = (index < processes.size()) ? processes[index].arrival_time : INT_MAX;
        int execute_time = min(current_process->remaining_time, next_time - current_time);
        
        current_process->remaining_time -= execute_time;
        current_time += execute_time;

        if (current_process->remaining_time == 0) {
            current_process->finish_time = current_time;
            if (last_process && last_process != current_process && current_time != next_time) {
                current_time += cs;
            }
        } else {
            pq.push(current_process);  // Reinsert the process back into the priority queue
        }
        last_process = current_process;
    }
}


void roundRobin(vector<Process>& processes, int cs, int quantum, Scheduler& scheduler) {
    queue<Process*> rq;  // ready queue
    vector<bool> hasStarted(processes.size(), false);
    int current_time = 0;
    Process* last_process = nullptr;

    // Load ready queue with initial processes available at start
    for (auto& process : processes) {
        if (process.arrival_time <= current_time) {
            rq.push(&process);
            hasStarted[process.id - 1] = true;
            if (process.start_time == -1) {
                process.start_time = current_time;
            }
        }
    }

    while (!rq.empty()) {
        Process* current_process = rq.front();
        rq.pop();

        if (last_process && last_process != current_process) {
            current_time += cs;  // Context switch time
        }

        // Start time for the first time this process gets the CPU
        if (!hasStarted[current_process->id - 1]) {
            current_process->start_time = current_time;
            hasStarted[current_process->id - 1] = true;
        }

        // Calculate the execution time
        int execution_time = min(current_process->remaining_time, quantum);
        current_process->remaining_time -= execution_time;
        current_time += execution_time;

        if (current_process->remaining_time > 0) {
            // Check for arrivals during this quantum
            for (auto& process : processes) {
                if (process.arrival_time <= current_time && !hasStarted[process.id - 1]) {
                    process.start_time = current_time;
                    hasStarted[process.id - 1] = true;
                    rq.push(&process);
                }
            }
            // Re-enqueue the current process
            rq.push(current_process);
        }
        else {
            // Process finishes
            current_process->finish_time = current_time;
        }

        last_process = current_process;
    }
}

int main() {
    Scheduler scheduler;
    readInput("processes.txt", scheduler);
    cout << "\nEnter Context Switch Time (ms): ";
    cin >> scheduler.context_switch;
    cout << "Enter Time Quantum for Round Robin (ms): ";
    cin >> scheduler.quantum;

    while (true) {
        int choice;
        cout << "\nChoose the scheduling algorithm or exit:\n";
        cout << "1. FCFS (First-Come, First-Served)\n";
        cout << "2. SRT (Shortest Remaining Time)\n";
        cout << "3. Round Robin\n";
        cout << "4. Exit\n> ";
        cin >> choice;

        switch (choice) {
        case 1:
            fcfs(scheduler.processes, scheduler.context_switch, scheduler);
            printGanttChart(scheduler.processes);
            calculateMetrics(scheduler.processes, scheduler);
            break;
        case 2:
            srt(scheduler.processes, scheduler.context_switch, scheduler);
            printGanttChart(scheduler.processes);
            calculateMetrics(scheduler.processes, scheduler);
            break;
        case 3:
            roundRobin(scheduler.processes, scheduler.context_switch, scheduler.quantum, scheduler);
            printGanttChart(scheduler.processes);
            calculateMetrics(scheduler.processes, scheduler);
            break;
        case 4:
            cout << "Exiting program." << endl;
            return 0;
        default:
            cout << "Invalid option. Please try again." << endl;
        }
    }

    return 0;
}