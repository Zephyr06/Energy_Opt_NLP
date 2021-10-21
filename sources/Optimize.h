#pragma once

#include <chrono>
#include <math.h>

#pragma once

#include "Parameters.h"
#include <Eigen/Dense>
#include "Declaration.h"
#include "Tasks.h"
#include "RTA_LL.h"
#include "Energy.h"
#include "utils.h"

using namespace std;
using namespace gtsam;

#define numberOfTasksNeedOptimize (N - lastTaskDoNotNeedOptimize - 1)

double valueGlobalOpt = INT64_MAX;
VectorDynamic vectorGlobalOpt;

struct OptimizeResult
{
    double initialError;
    double optimizeError;
    VectorDynamic initialVariable;
    VectorDynamic optimizeVariable;
    OptimizeResult() : initialError(-1), optimizeError(-1)
    {
        ;
    }
    OptimizeResult(double ie, double oe, VectorDynamic iv, VectorDynamic ov) : initialError(ie), optimizeError(oe), initialVariable(iv), optimizeVariable(ov) {}
};

template <class Schedul_Analysis>
class Energy_Opt
{
public:
    class ComputationFactor : public NoiseModelFactor1<VectorDynamic>
    {
    public:
        TaskSet tasks_;
        int lastTaskDoNotNeedOptimize;
        // int numberOfTasksNeedOptimize;
        VectorDynamic responseTimeInitial;
        int N;

        ComputationFactor(Key key, TaskSet &tasks, int lastTaskDoNotNeedOptimize, VectorDynamic responseTimeInitial,
                          SharedNoiseModel model) : NoiseModelFactor1<VectorDynamic>(model, key),
                                                    tasks_(tasks), lastTaskDoNotNeedOptimize(lastTaskDoNotNeedOptimize),
                                                    responseTimeInitial(responseTimeInitial)
        {
            N = tasks_.size();
            TASK_NUMBER = N;
            // numberOfTasksNeedOptimize = N - lastTaskDoNotNeedOptimize - 1;
        }
        boost::function<Matrix(const VectorDynamic &)> f =
            [this](const VectorDynamic &executionTimeVector)
        {
            bool flagSchedulable = true;
            double currentEnergyConsumption = 0;

            VectorDynamic err;
            err.resize(numberOfTasksNeedOptimize, 1);
            err.setZero();
            TaskSet taskSetCurr_ = tasks_;
            UpdateTaskSetExecutionTime(taskSetCurr_, executionTimeVector, lastTaskDoNotNeedOptimize);

            vector<Task> hpTasks;
            for (int i = 0; i < lastTaskDoNotNeedOptimize + 1; i++)
            {
                hpTasks.push_back(taskSetCurr_[i]);
            }
            // cout << "The response time and deadline for each task is: " << endl;
            // for (int i = lastTaskDoNotNeedOptimize + 1; i < N; i++)
            int startIndex;
            if (Schedul_Analysis::type() == "LL")
                ;
            else if (Schedul_Analysis::type() == "WAP")
            {
                for (int i = 0; i < lastTaskDoNotNeedOptimize + 1; i++)
                {
                    double responseTime = Schedul_Analysis::RTA_Common_Warm(responseTimeInitial(i, 0), taskSetCurr_, i);
                    err(0, 0) += Barrier(tasks_[i].deadline - responseTime);
                }
            }
            else
                CoutError("Undefined type() in Schedul_Analysis, Optimize()!");
            for (int i = lastTaskDoNotNeedOptimize + 1; i < N; i++)
            {
                // energy part
                double frequency = tasks_[i].executionTime / taskSetCurr_[i].executionTime;
                err(i - (lastTaskDoNotNeedOptimize + 1), 0) += 1.0 / tasks_[i].period * EstimateEnergyTask(tasks_[i], frequency);
                currentEnergyConsumption += err(i - (lastTaskDoNotNeedOptimize + 1), 0);
                // barrier function part
                double responseTime = Schedul_Analysis::RTA_Common_Warm(responseTimeInitial(i, 0), taskSetCurr_, i);
                // double responseTime = Schedul_Analysis::ResponseTimeAnalysisWarm(responseTimeInitial(i, 0), taskSetCurr_[i], hpTasks);
                // cout << responseTime << ", " << taskSetCurr_[i].deadline << endl;

                err(i - (lastTaskDoNotNeedOptimize + 1), 0) += Barrier(tasks_[i].deadline - responseTime);
                if (enableMaxComputationTimeRestrict)
                    err(i - (lastTaskDoNotNeedOptimize + 1), 0) += Barrier(tasks_[i].executionTime * 2 -
                                                                           executionTimeVector(i - (lastTaskDoNotNeedOptimize + 1), 0));

                if (tasks_[i].deadline - responseTime < 0 ||
                    executionTimeVector(i - (lastTaskDoNotNeedOptimize + 1) > tasks_[i].executionTime * 2))
                    flagSchedulable = false;
                // err(i - (lastTaskDoNotNeedOptimize + 1), 0) += Barrier(tasks_[i].executionTime - taskSetCurr_[i].executionTime);
                hpTasks.push_back(taskSetCurr_[i]);
            }

            // check schedulability
            if (flagSchedulable && currentEnergyConsumption / weightEnergy < valueGlobalOpt)
            {
                // update globalOptVector
                valueGlobalOpt = currentEnergyConsumption / weightEnergy;
                for (int i = lastTaskDoNotNeedOptimize + 1; i < N; i++)
                    vectorGlobalOpt(i, 0) = executionTimeVector(i - lastTaskDoNotNeedOptimize - 1, 0);
                // if (debugMode == 1)
                //     cout << "vectorGlobalOpt is " << vectorGlobalOpt << endl;
            }

            return err;
        };
        Vector evaluateError(const VectorDynamic &executionTimeVector, boost::optional<Matrix &> H = boost::none) const override
        {

            boost::function<Matrix(const VectorDynamic &)> f2 =
                [this](const VectorDynamic &executionTimeVector)
            {
                VectorDynamic err;
                err.resize(numberOfTasksNeedOptimize, 1);
                TaskSet taskSetCurr_ = tasks_;
                UpdateTaskSetExecutionTime(taskSetCurr_, executionTimeVector, lastTaskDoNotNeedOptimize);

                vector<Task> hpTasks;
                for (int i = 0; i < lastTaskDoNotNeedOptimize + 1; i++)
                {
                    hpTasks.push_back(taskSetCurr_[i]);
                }
                // cout << "The response time and deadline for each task is: " << endl;
                for (int i = lastTaskDoNotNeedOptimize + 1; i < N; i++)
                {
                    // energy part
                    double frequency = tasks_[i].executionTime / taskSetCurr_[i].executionTime;
                    err(i - (lastTaskDoNotNeedOptimize + 1), 0) = 1.0 / tasks_[i].period * EstimateEnergyTask(tasks_[i], frequency);
                }
                return err;
            };

            VectorDynamic err;
            err = f(executionTimeVector);

            if (H)
            {
                if (exactJacobian)
                    *H = NumericalDerivativeDynamicUpper(f, executionTimeVector, deltaOptimizer, numberOfTasksNeedOptimize);
                else
                    *H = NumericalDerivativeDynamicUpper(f2, executionTimeVector, deltaOptimizer, numberOfTasksNeedOptimize);
                // *H = NumericalDerivativeDynamic(f2, executionTimeVector, deltaOptimizer, numberOfTasksNeedOptimize);
                // *H = jacobian;
                if (debugMode == 1)
                {
                    cout << endl;
                    cout << "The current evaluation point is " << endl
                         << executionTimeVector << endl;
                    cout << "The Jacobian is " << endl
                         << *H << endl;
                    // cout << "The approximated Jacobian is " << endl
                    //      << jacobian << endl;
                    cout << "The current error is " << endl
                         << err << endl
                         << endl
                         << err.norm() << endl
                         << endl;
                }

                if (debugMode == 1)
                {
                    TaskSet taskSetCurr_ = tasks_;
                    UpdateTaskSetExecutionTime(taskSetCurr_, executionTimeVector, lastTaskDoNotNeedOptimize);
                    vector<Task> hpTasks;
                    for (int i = 0; i < lastTaskDoNotNeedOptimize + 1; i++)
                    {
                        hpTasks.push_back(taskSetCurr_[i]);
                    }
                    cout << Color::green << "The response time and deadline for each task is: " << Color::def << endl;
                    for (int i = lastTaskDoNotNeedOptimize + 1; i < N; i++)
                    {
                        // energy part
                        // double frequency = tasks_[i].executionTime / taskSetCurr_[i].executionTime;
                        // err(i - (lastTaskDoNotNeedOptimize + 1), 0) = hyperPeriod / tasks_[i].period * EstimateEnergyTask(tasks_[i], frequency);
                        // barrier function part

                        double responseTime = Schedul_Analysis::RTA_Common_Warm(responseTimeInitial(i, 0), taskSetCurr_, i);
                        // double responseTime = Schedul_Analysis::ResponseTimeAnalysisWarm(responseTimeInitial(i, 0), taskSetCurr_[i], hpTasks);
                        cout << responseTime << ", " << taskSetCurr_[i].deadline << endl;
                        // err(i - (lastTaskDoNotNeedOptimize + 1), 0) += Barrier(tasks_[i].deadline - responseTime);
                        hpTasks.push_back(taskSetCurr_[i]);
                    }
                }
            }
            return err;
        }
    };

    // ------------------
    // TODO: clamp turn 123.99 to 124 rather than 123
    static VectorDynamic ClampComputationTime(VectorDynamic comp, TaskSet &tasks, int lastTaskDoNotNeedOptimize,
                                              VectorDynamic &responseTimeInitial, string roundType = roundTypeInClamp)
    {
        int n = comp.rows();
        for (int i = 0; i < n; i++)
            comp(i, 0) = int(comp(i, 0));
        if (roundType == "rough")
        {
            return comp;
        }
        else if (roundType == "fine")
        {
            int N = tasks.size();
            // VectorDynamic warmStart = ResponseTimeOfTaskSetHard<Schedul_Analysis>(tasks, comp);

            vector<pair<int, double>> objectiveVec;
            // objectiveVec.reserve(N);
            for (int i = lastTaskDoNotNeedOptimize + 1; i < N; i++)
            {
                objectiveVec.push_back(make_pair(i, JacobianInClamp(tasks, comp, i)));
            }
            sort(objectiveVec.begin(), objectiveVec.end(), comparePair);
            int minEliminate = INT32_MAX;

            int iterationNumber = 0;
            TaskSet taskDuringOpt = tasks;
            UpdateTaskSetExecutionTime(taskDuringOpt, comp, lastTaskDoNotNeedOptimize);

            if (debugMode == 1)
            {
                cout << "before binary search, here is the task set" << endl;
                for (int i = 0; i < N; i++)
                    taskDuringOpt[i].print();
            }

            // int left = 0, right = 0;
            while (objectiveVec.size() > 0)
            {
                int currentIndex = objectiveVec[0].first;

                // try to round up, if success, keep the loop; otherwise, eliminate it and high priority tasks
                // can be speeded up, if necessary, by binary search
                int left = comp(currentIndex, 0);
                // int right = min(taskDuringOpt[currentIndex].deadline,
                //                 int(tasks[currentIndex].executionTime * computationBound));
                int right = taskDuringOpt[currentIndex].deadline;
                if (enableMaxComputationTimeRestrict)
                {
                    right = min(right, tasks[currentIndex].executionTime * computationBound);
                }
                // for (int j = 0; j < currentIndex; j++)
                //     right -= taskDuringOpt[j].executionTime;
                if (left > right)
                {
                    cout << "left > right error in clamp!" << endl;
                    throw;
                }
                int ref = comp(currentIndex, 0);

                while (left <= right)
                {
                    int mid = (left + right) / 2;

                    taskDuringOpt[currentIndex].executionTime = mid;

                    if ((not CheckSchedulability<Schedul_Analysis>(taskDuringOpt, responseTimeInitial)) ||
                        not WithInBound(tasks, taskDuringOpt))
                    {
                        taskDuringOpt[currentIndex].executionTime = ref;
                        comp(currentIndex, 0) = ref;
                        taskDuringOpt[currentIndex].executionTime = comp(currentIndex, 0);
                        minEliminate = currentIndex;
                        for (int i = objectiveVec.size() - 1; i > lastTaskDoNotNeedOptimize; i--)
                        {
                            if (objectiveVec[i].first <= minEliminate)
                            {
                                objectiveVec.erase(objectiveVec.begin() + i);
                            }
                        }
                        right = mid - 1;
                    }
                    else if (left == right)
                    {
                        minEliminate = currentIndex;
                        for (int i = objectiveVec.size() - 1; i > lastTaskDoNotNeedOptimize; i--)
                        {
                            if (objectiveVec[i].first <= minEliminate)
                            {
                                objectiveVec.erase(objectiveVec.begin() + i);
                            }
                        }
                        break;
                    }
                    else
                    {
                        comp(currentIndex, 0) = mid;
                        taskDuringOpt[currentIndex].executionTime = mid;
                        ref = mid;
                        left = mid + 1;
                    }
                }

                iterationNumber++;
                if (iterationNumber > N)
                {
                    cout << red << "iterationNumber error in Clamp!" << def << endl;
                    // throw;
                    break;
                }
            };
        }
        else
        {
            cout << "input error in ClampComputationTime" << endl;
            throw;
        }

        return comp;
    }

    /**
     * find the tasks that do not need to optimize;
     * i means i-th task do not need optimization,  while i+1, ..., N need
     * -1 means all tasks need optimization
     * N-1 means all tasks do not need optimization
     **/
    static int FindTaskDoNotNeedOptimize(const TaskSet &tasks, VectorDynamic computationTimeVector, int lastTaskDoNotNeedOptimize,
                                         VectorDynamic computationTimeWarmStart, double tolerance = eliminateVariableThreshold)
    {
        // update the tasks with the new optimal computationTimeVector
        TaskSet tasksCurr = tasks;
        UpdateTaskSetExecutionTime(tasksCurr, computationTimeVector);
        // cout << "eliminateTol" << eliminateTol << endl;
        int N = tasks.size();
        vector<Task> hpTasks = tasksCurr;
        for (int i = N - 1; i >= 0; i--)
        {
            hpTasks.pop_back();
            tasksCurr[i].executionTime += eliminateTol;

            double rt = Schedul_Analysis::RTA_Common_Warm(computationTimeWarmStart(i, 0), tasksCurr, i);
            // double rt = Schedul_Analysis::ResponseTimeAnalysisWarm(computationTimeWarmStart(i, 0), tasksCurr[i], hpTasks);
            // cout << "rt is " << rt << " deadline is " << tasks[i].deadline << endl;
            if (abs(rt - tasks[i].deadline) <= tolerance || rt > tasks[i].deadline ||
                computationTimeVector(i, 0) + tolerance > tasks[i].executionTime * 2)
                return i;
            tasksCurr[i].executionTime -= eliminateTol;
        }
        return -1;
    }

    static VectorDynamic UnitOptimization(TaskSet &tasks,
                                          int lastTaskDoNotNeedOptimize, VectorDynamic &initialEstimate,
                                          VectorDynamic &responseTimeInitial)
    {
        int N = tasks.size();
        TASK_NUMBER = N;

        // build the factor graph
        auto model = noiseModel::Isotropic::Sigma(numberOfTasksNeedOptimize, noiseModelSigma);
        NonlinearFactorGraph graph;
        Symbol key('a', 0);
        graph.emplace_shared<ComputationFactor>(key, tasks, lastTaskDoNotNeedOptimize, responseTimeInitial, model);

        Values initialEstimateFG;
        initialEstimateFG.insert(key, initialEstimate);

        // usually, when the change of variables between steps is smaller than 1,
        // we can already terminate; the corresponding minimal of relative error is
        // approximately 2e-3;

        Values result;
        if (optimizerType == 1)
        {
            DoglegParams params;
            if (debugMode == 1)
                params.setVerbosityDL("VERBOSE");
            params.setDeltaInitial(deltaInitialDogleg);
            params.setRelativeErrorTol(relativeErrorTolerance);
            DoglegOptimizer optimizer(graph, initialEstimateFG, params);
            result = optimizer.optimize();
        }
        else if (optimizerType == 2)
        {
            LevenbergMarquardtParams params;
            params.setlambdaInitial(initialLambda);
            if (debugMode == 1)
                params.setVerbosityLM("DAMPED");
            params.setlambdaLowerBound(lowerLambda);
            params.setlambdaUpperBound(upperLambda);
            params.setRelativeErrorTol(relativeErrorTolerance);
            LevenbergMarquardtOptimizer optimizer(graph, initialEstimateFG, params);
            result = optimizer.optimize();
        }

        VectorDynamic optComp = result.at<VectorDynamic>(key);
        if (debugMode == 1)
        {
            cout << "After optimization, the computation time vector is " << optComp << endl;
        }

        // if (debugMode == 1)
        //     cout << "After clamp, the computation time vector is " << optComp << endl;
        return optComp;
    }

    /**
 * tasksDuringOpt's tasks are already updated with latest executionTime;
 * this function performs ipm optimization, i.e., adjusting weight for better performance 
 * at float-point percesion; 
 * 
 * once elimination condition changes, the function returns
 */
    static VectorDynamic UnitOptimizationIPM(TaskSet &tasksDuringOpt, int lastTaskDoNotNeedOptimize,
                                             VectorDynamic &initialEstimate,
                                             VectorDynamic &responseTimeInitial,
                                             VectorDynamic computationTimeVectorLocalOpt)
    {
        int N = tasksDuringOpt.size();
        VectorDynamic initialVar = initialEstimate;
        VectorDynamic variNew = initialVar;
        weightEnergy = minWeightToBegin;
        if (not enableIPM)
        {
            try
            {
                variNew = UnitOptimization(tasksDuringOpt, lastTaskDoNotNeedOptimize,
                                           initialVar, responseTimeInitial);
            }
            catch (...)
            {
                cout << green << "Catch some error, most probably indetermined Jacobian error" << def << endl;
                computationTimeVectorLocalOpt = vectorGlobalOpt;
                for (int i = lastTaskDoNotNeedOptimize + 1; i < N; i++)
                    variNew(i - lastTaskDoNotNeedOptimize - 1, 0) = vectorGlobalOpt(i, 0);
            }
            return variNew;
        }

        double resOld = EstimateEnergyTaskSet(tasksDuringOpt, initialEstimate,
                                              lastTaskDoNotNeedOptimize)
                            .sum() /
                        weightEnergy;
        double resNew = resOld;
        double initialEnergy = resOld;

        // iterations
        int iterationNumIPM = 0;
        do
        {
            // Problem:
            // - batch doesn't work, while opt does?
            if (debugMode == 1)
                cout << "Current weightEnergy " << weightEnergy << endl;
            resOld = resNew;
            initialVar = variNew;
            try
            {
                variNew = UnitOptimization(tasksDuringOpt, lastTaskDoNotNeedOptimize,
                                           initialVar, responseTimeInitial);
            }
            catch (...)
            {
                cout << green << "Catch some error, most probably indetermined Jacobian error" << def << endl;
                computationTimeVectorLocalOpt = vectorGlobalOpt;
                for (int i = lastTaskDoNotNeedOptimize + 1; i < N; i++)
                    variNew(i - lastTaskDoNotNeedOptimize - 1, 0) = vectorGlobalOpt(i, 0);
            }

            resNew = EstimateEnergyTaskSet(tasksDuringOpt, variNew, lastTaskDoNotNeedOptimize).sum() /
                     weightEnergy;
            weightEnergy *= weightStep;
            punishmentInBarrier *= weightStep;
            if (debugMode == 1)
                cout << "After one iteration of inside IPM, the current ratio is "
                     << resNew / initialEnergy << " Iteration number is " << iterationNumIPM << endl;
            iterationNumIPM++;
        } while (!checkConvergenceInterior(resOld, initialVar, resNew, variNew, relErrorTolIPM, xTolIPM) &&
                 lastTaskDoNotNeedOptimize ==
                     FindTaskDoNotNeedOptimize(tasksDuringOpt,
                                               computationTimeVectorLocalOpt,
                                               lastTaskDoNotNeedOptimize, responseTimeInitial) &&
                 iterationNumIPM <= iterationNumIPM_Max);
        return variNew;
    }

    /**
 * Perform optimization for one task set;
 * this function only performs optimization and elimination, it does not change weights
 **/
    static double OptimizeTaskSetOneIte(TaskSet &tasks)
    {
        int N = tasks.size();
        // vectorGlobalOpt.resize(N, 1);

        // this function also checks schedulability
        VectorDynamic responseTimeInitial = ResponseTimeOfTaskSetHard<Schedul_Analysis>(tasks);
        if (responseTimeInitial(0, 0) == -1)
            return -2;

        VectorDynamic initialExecutionTime = GetParameterVD<int>(tasks, "executionTime");

        int lastTaskDoNotNeedOptimize = FindTaskDoNotNeedOptimize(tasks, initialExecutionTime, 0, responseTimeInitial);
        if (lastTaskDoNotNeedOptimize == N - 1)
            return 1;

        // its size is always N
        VectorDynamic computationTimeVectorLocalOpt = initialExecutionTime;
        vectorGlobalOpt = initialExecutionTime;

        int numberOfIteration = 0;
        TaskSet tasksDuringOpt = tasks;

        do
        {
            VectorDynamic initialEstimateDuringOpt;
            initialEstimateDuringOpt.resize(numberOfTasksNeedOptimize, 1);
            for (int i = lastTaskDoNotNeedOptimize + 1; i < N; i++)
                initialEstimateDuringOpt(i - lastTaskDoNotNeedOptimize - 1, 0) =
                    computationTimeVectorLocalOpt(i, 0);

            // perform optimization
            VectorDynamic optComp = UnitOptimizationIPM(tasksDuringOpt, lastTaskDoNotNeedOptimize,
                                                        initialEstimateDuringOpt, responseTimeInitial,
                                                        computationTimeVectorLocalOpt);
            // formulate new computationTime
            for (int i = lastTaskDoNotNeedOptimize + 1; i < N; i++)
                computationTimeVectorLocalOpt(i, 0) = optComp(i - lastTaskDoNotNeedOptimize - 1, 0);

            computationTimeVectorLocalOpt = ClampComputationTime(computationTimeVectorLocalOpt, tasks,
                                                                 lastTaskDoNotNeedOptimize, responseTimeInitial, "rough");
            // cout << computationTimeVectorLocalOpt << endl;
            // find variables to eliminate
            int adjustEliminateTolNum = 0;
            // cout << eliminateTol << endl;
            int lastTaskDoNotNeedOptimizeAfterOpt;
            while (adjustEliminateTolNum < 10)
            {
                lastTaskDoNotNeedOptimizeAfterOpt = FindTaskDoNotNeedOptimize(
                    tasksDuringOpt,
                    computationTimeVectorLocalOpt, lastTaskDoNotNeedOptimize, responseTimeInitial);
                if (lastTaskDoNotNeedOptimizeAfterOpt == lastTaskDoNotNeedOptimize)
                    eliminateTol *= eliminateStep;
                adjustEliminateTolNum++;
            }

            if (debugMode == 1)
            {
                cout << "After one iteration, the computationTimeVectorLocalOpt is " << computationTimeVectorLocalOpt << endl;
                cout << "After one iteration, the vectorGlobalOpt is " << vectorGlobalOpt << endl;

                TaskSet tasks2 = tasks;
                for (int i = 0; i < N; i++)
                    tasks2[i].executionTime = computationTimeVectorLocalOpt(i, 0);
                VectorDynamic ttt = ResponseTimeOfTaskSetHard<Schedul_Analysis>(tasks2);
            }

            // check optimization results to see if there are tasks to remove further
            for (int i = lastTaskDoNotNeedOptimize + 1; i <= lastTaskDoNotNeedOptimizeAfterOpt; i++)
            {
                tasksDuringOpt[i].executionTime = computationTimeVectorLocalOpt(i, 0);
            }
            lastTaskDoNotNeedOptimize = lastTaskDoNotNeedOptimizeAfterOpt;

            numberOfIteration++;
            if (numberOfIteration > 4 * N)
            {
                // cout << red << "numberOfIteration error!\n"
                //      << def << endl;
                // if (debugMode == 1)
                //     Print(tasks);
                // throw;
                CoutWarning("numberOfIteration reaches the maximum limits, the algorithm decides to give up!");
            }
        } while (numberOfTasksNeedOptimize > 0);

        // performance evaluation
        if (CheckSchedulability<Schedul_Analysis>(tasksDuringOpt))
        {
            if (debugMode == 1)
            {
                cout << "The task set is schedulable after optimization\n";
                cout << endl;
                cout << "The original task set is: " << endl;
                for (int i = 0; i < N; i++)
                {
                    cout << i << " ";
                    tasks[i].print();
                }
            }
            if (debugMode == 1)
                cout << "computationTimeVectorLocalOpt before Clamp fine: " << computationTimeVectorLocalOpt << endl;
            computationTimeVectorLocalOpt = ClampComputationTime(computationTimeVectorLocalOpt, tasks, -1,
                                                                 responseTimeInitial, roundTypeInClamp);
            if (debugMode == 1)
                cout << "computationTimeVectorLocalOpt after Clamp fine: " << computationTimeVectorLocalOpt << endl;
            double initialEnergyCost = EstimateEnergyTaskSet(tasks, initialExecutionTime).sum();
            double afterEnergyCost = EstimateEnergyTaskSet(tasks, computationTimeVectorLocalOpt).sum();
            if (debugMode == 1)
            {
                cout << "Normalized objective function after optimization is " << afterEnergyCost << endl;
                cout << "The variable after optimization is " << computationTimeVectorLocalOpt << endl;
            }
            if (runMode == "compare")
                return afterEnergyCost / weightEnergy;
            else if (runMode == "normal")
                return afterEnergyCost / initialEnergyCost;
            else
            {
                cout << red << "Unrecognized runMode!!" << def << endl;
                throw;
            }
        }
        else
        {
            cout << "Unfeasible!" << endl;
            return -1;
        }
        return -1;
    }

    /**
 * initialize all the global variables
 */
    static double OptimizeTaskSet(TaskSet &tasks)
    {
        int N = tasks.size();
        vectorGlobalOpt.resize(N, 1);
        vectorGlobalOpt.setZero();
        valueGlobalOpt = INT64_MAX;
        weightEnergy = minWeightToBegin;

        double eliminateTolRef = eliminateTol;

        double res = OptimizeTaskSetOneIte(tasks);
        // Some variables become 0, which actually means a failure
        if (isinf(res))
            res = 10;
        eliminateTol = eliminateTolRef;
        return res;
    }
};