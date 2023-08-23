#include "sources/ControlOptimization/ControlOptimizeNasri19.h"
#include "sources/RTA/RTA_Nasri19.h"
#include "sources/Tools/profilier.h"
using namespace rt_num_opt;
using namespace ControlOptimize;
using namespace std;
TEST(case1, v1) {
    BeginTimer("main");

    std::string path = "/home/zephyr/Programming/Energy_Opt_NLP/TaskData/" +
                       testDataSetName + ".yaml";

    rt_num_opt::DAG_Nasri19 tasks_dag = rt_num_opt::ReadDAGNasri19_Tasks(path);
    TaskSet tasks = tasks_dag.tasks_;
    VectorDynamic coeff = ReadControlCoeff(path);

    std::vector<bool> maskForElimination(tasks_dag.SizeNode(), false);

    auto sth =
        OptimizeTaskSetIterative<FactorGraphNasri<DAG_Nasri19, RTA_Nasri19>,
                                 DAG_Nasri19, RTA_Nasri19>(tasks_dag, coeff,
                                                           maskForElimination);

    // FindEliminatedVariables(tasks, maskForElimination);
    // AssertEqualVectorExact({true, false, false, false, false},
    // maskForElimination);
    cout << "Initial objective function is "
         << FactorGraphNasri<DAG_Nasri19, RTA_Nasri19>::RealObj(
                rt_num_opt::ReadDAGNasri19_Tasks(path), coeff)
         << endl;
    cout << "After optimization, the period vector is " << sth.first << endl;
    cout << "After optimization, the objective function is " << sth.second
         << endl;
    EndTimer("main");
    PrintTimer();
    std::cout << count1 << ", " << count2 << std::endl;
}

int main() {
    TestResult tr;
    return TestRegistry::runAllTests(tr);
}
