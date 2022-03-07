#pragma once

#include "Tasks.h"
#include "Declaration.h"
/**
 * @brief All customized TaskSetType must inherit from TaskSetNormal in Tasks.h
 * 
 */

/**
 * @brief RTA_BASE encapsulate all the interafaces required
 *  by Energy_optimization.
 * All kinds of RTA should inherit 
 * from RTA_BASE and implement its virtual function
 * 
 */

template <class TaskSetType>
class RTA_BASE
{
public:
    TaskSetType tasks;
    RTA_BASE() {}
    RTA_BASE(const TaskSetType &tasks) : tasks(tasks) {}

    /**
     * @brief 
     * 
     * @param tasks inverse priority ordered
     * @param index 
     * @return double 
     */
    virtual double RTA_Common_Warm(double beginTime, int index)
    {
        CoutError("Calling RTA_Common_Warm that is supposed to be overwriten!");
        return 0;
    }

    //*** the following functions are actually used in optimzie.h

    VectorDynamic ResponseTimeOfTaskSet(const VectorDynamic &warmStart)
    {
        int N = tasks.tasks_.size();
        VectorDynamic res = GenerateVectorDynamic(N);
        if (printRTA)
        {
            cout << "Response time analysis of the task set is:" << endl;
        }
        for (int i = 0; i < N; i++)
        {
            res(i, 0) = RTA_Common_Warm(warmStart(i, 0), i);
            if (printRTA)
            {
                cout << "Task " << i << ": " << res(i, 0) << endl;
            }
            if (res(i, 0) >= INT32_MAX - 10000)
            {
                int a = 1;
                a *= a;
            }
        }
        return res;
    }

    VectorDynamic ResponseTimeOfTaskSet()
    {
        VectorDynamic warmStart = GetParameterVD<double>(tasks, "executionTime");
        return ResponseTimeOfTaskSet(warmStart);
    }
    /**
     * @brief 
     * 
     * @tparam Schedul_Analysis 
     * @param tasks 
     * @param warmStart 
     * @param whetherPrint 
     * @param tol positive value, makes schedulability check more strict
     * @return true: system is schedulable
     * @return false: system is not schedulable
     */

    bool CheckSchedulability(VectorDynamic warmStart,
                             bool whetherPrint = false, double tol = 0)
    {
        int N = tasks.tasks_.size();
        for (int i = 0; i < N; i++)
        {
            double rta = RTA_Common_Warm(warmStart(i, 0), i);
            if (whetherPrint)
                cout << "response time for task " << i << " is " << rta << " and deadline is "
                     << min(tasks.tasks_[i].deadline, tasks.tasks_[i].period) << endl;
            if (rta + tol > min(tasks.tasks_[i].deadline, tasks.tasks_[i].period))
            {
                if (whetherPrint)
                    cout << "The current task set is not schedulable!\n";
                return false;
            }
        }
        if (whetherPrint)
            cout << endl;
        return true;
    }

    bool CheckSchedulability(bool whetherPrint = false)
    {
        VectorDynamic warmStart = GetParameterVD<double>(tasks.tasks_, "executionTime");
        return CheckSchedulability(warmStart, whetherPrint);
    }

    bool CheckSchedulabilityDirect(const VectorDynamic &rta)
    {
        int N = tasks.tasks_.size();
        for (int i = 0; i < N; i++)
        {
            if (rta(i, 0) > min(tasks.tasks_[i].deadline, tasks.tasks_[i].period))
            {
                return false;
            }
        }
        return true;
    }
};
