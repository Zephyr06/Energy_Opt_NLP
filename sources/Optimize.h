

#include <gtsam/base/numericalDerivative.h>
#include <gtsam/geometry/Point3.h>
#include <gtsam/inference/Key.h>
#include <gtsam/inference/Symbol.h>
#include <gtsam/linear/VectorValues.h>
#include <gtsam/nonlinear/GaussNewtonOptimizer.h>
#include <gtsam/nonlinear/LevenbergMarquardtOptimizer.h>
#include <gtsam/nonlinear/NonlinearFactorGraph.h>
#include <gtsam/nonlinear/Values.h>

#include "Tasks.h"
#include "ResponseTimeAnalysis.h"
#include "Energy.h"
#include "Parameters.h"

using namespace std;
using namespace gtsam;

/**
 * barrier function for the optimization
 **/
float Barrier(float x)
{
    if (x >= 0)
        return pow(x, 2);
    else
        return 1e4 * pow(10, TASK_NUMBER - 3) * pow(1 - x, 1);
}

void UpdateTaskSetExecutionTime(TaskSet &taskSet, ComputationTimeVector executionTimeVec)
{
    int i = 0;
    for (auto &task : taskSet)
        task.executionTime = executionTimeVec(i++, 0);
}

class ComputationFactor : public NoiseModelFactor1<ComputationTimeVector>
{
public:
    TaskSet tasks_;

    ComputationFactor(Key key, TaskSet &tasks,
                      SharedNoiseModel model) : NoiseModelFactor1<ComputationTimeVector>(model, key),
                                                tasks_(tasks) {}

    Vector evaluateError(const ComputationTimeVector &executionTimeVector, boost::optional<Matrix &> H = boost::none) const override
    {
        ErrElement err;
        TaskSet taskSetCurr_ = tasks_;
        UpdateTaskSetExecutionTime(taskSetCurr_, executionTimeVector);

        err = EstimateEnergyTaskSet(tasks_, executionTimeVector);
        // todo: find a way to test the evaluteError function
        for (int i = 0; i < TASK_NUMBER; i++)
        {
            Task taskCurr_(tasks_[i]);

            taskCurr_.executionTime = executionTimeVector(i, 0);
            vector<Task> hpTasks;
            for (int j = 0; j < i; j++)
                hpTasks.push_back(taskSetCurr_[j]);

            float responseTime = ResponseTimeAnalysis<float>(taskCurr_, hpTasks);

            err(i, 0) += Barrier(tasks_[i].deadline - responseTime);
        }

        if (H)
        {
            boost::function<Matrix(const ComputationTimeVector &)> f =
                [this](const ComputationTimeVector &input)
            {
                ErrElement err;
                TaskSet taskSetCurr_ = tasks_;
                UpdateTaskSetExecutionTime(taskSetCurr_, input);

                // todo: find a way to test the evaluteError function
                err = EstimateEnergyTaskSet(tasks_, input);
                // todo: find a way to test the evaluteError function
                for (int i = 0; i < TASK_NUMBER; i++)
                {
                    Task taskCurr_(tasks_[i]);

                    taskCurr_.executionTime = input(i, 0);
                    vector<Task> hpTasks;
                    for (int j = 0; j < i; j++)
                        hpTasks.push_back(taskSetCurr_[j]);

                    float responseTime = ResponseTimeAnalysis<float>(taskCurr_, hpTasks);

                    err(i, 0) += Barrier(tasks_[i].deadline - responseTime);
                }
                return err;
            };

            *H = numericalDerivative11(f, executionTimeVector,
                                       deltaOptimizer);
            cout << "The Jacobian is " << endl
                 << *H << endl;
        }
        return err;
    }
};

ComputationTimeVector InitializeOptimization(const TaskSet &tasks)
{
    ComputationTimeVector comp;
    for (int i = 0; i < TASK_NUMBER; i++)
        comp(i, 0) = tasks[i].executionTime;
    return comp;
}

/**
 * Perform optimization for one task set
 **/
float OptimizeTaskSet(TaskSet &tasks)
{
    // test schedulability
    if (!CheckSchedulability<int>(tasks))
    {
        cout << "The task set is not schedulable!\n";
        return -1;
    }

    // build the factor graph since there

    auto model = noiseModel::Isotropic::Sigma(3, noiseModelSigma);

    NonlinearFactorGraph graph;
    Symbol key('a', 0);

    graph.emplace_shared<ComputationFactor>(key, tasks, model);

    // ComputationTimeVector comp;
    // comp << 10, 20, 30;
    Values initialEstimate;
    initialEstimate.insert(key, InitializeOptimization(tasks));

    LevenbergMarquardtParams params;
    params.setlambdaInitial(initialLambda);
    params.setVerbosityLM("SUMMARY");
    LevenbergMarquardtOptimizer optimizer(graph, initialEstimate, params);

    Values result = optimizer.optimize();

    ComputationTimeVector optComp = result.at<ComputationTimeVector>(key);
    cout << "After optimization, the computation time vector is " << optComp << endl;

    double initialEnergyCost = EstimateEnergyTaskSet(tasks, InitializeOptimization(tasks)).sum();
    double afterEnergyCost = EstimateEnergyTaskSet(tasks, optComp).sum();
    return afterEnergyCost / initialEnergyCost;
}