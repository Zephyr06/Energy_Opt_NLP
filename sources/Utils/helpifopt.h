#pragma once
#include <ifopt/variable_set.h>
#include <ifopt/constraint_set.h>
#include <ifopt/cost_term.h>

#include <boost/optional/optional.hpp>
#include <boost/optional/optional_io.hpp>

#include "sources/MatrixConvenient.h"
#include "sources/Utils/Parameters.h"
#include "sources/RTA/RTA_LL.h"
#include "sources/EnergyOptimization/Energy.h"

namespace rt_num_opt
{
    void Eigen2Array(VectorDynamic &x, double res[])
    {
        for (int i = 0; i < x.rows(); i++)
            res[i] = x(i);
    }

    template <class TaskSetType, class Schedul_Analysis>
    class ExConstraint : public ifopt::ConstraintSet
    {
    public:
        TaskSetType taskGeneral_;

        ExConstraint(TaskSetType &tasks) : ExConstraint(tasks, "constraint1") {}

        // This constraint set just contains 1 constraint, however generally
        // each set can contain multiple related constraints.
        ExConstraint(TaskSetType &tasks, const std::string &name) : ConstraintSet(1, name), taskGeneral_(tasks) {}

        boost::function<gtsam::Matrix(const VectorDynamic &)> f =
            [this](const VectorDynamic &executionTimeVector)
        {
            TaskSetType taskT = taskGeneral_;
            UpdateTaskSetExecutionTime(taskT.tasks_, executionTimeVector);
            Schedul_Analysis r(taskT);
            if (r.CheckSchedulability())
            {
                return GenerateVectorDynamic1D(0);
            }
            else
            {
                return GenerateVectorDynamic1D(1);
            }
        };

        // The constraint value minus the constant value "1", moved to bounds.
        VectorDynamic GetValues() const override
        {
            VectorDynamic g(GetRows());
            VectorDynamic x = GetVariables()->GetComponent("var_set1")->GetValues();
            return f(x);
        };

        // The only constraint in this set is an equality constraint to 1.
        // Constant values should always be put into GetBounds(), not GetValues().
        // For inequality constraints (<,>), use Bounds(x, inf) or Bounds(-inf, x).
        VecBound GetBounds() const override
        {
            VecBound b(GetRows());
            b.at(0) = ifopt::Bounds(0, 0);
            return b;
        }

        // This function provides the first derivative of the constraints.
        // In case this is too difficult to write, you can also tell the solvers to
        // approximate the derivatives by finite differences and not overwrite this
        // function, e.g. in ipopt.cc::use_jacobian_approximation_ = true
        // Attention: see the parent class function for important information on sparsity pattern.
        void FillJacobianBlock(std::string var_set, Jacobian &jac_block) const override
        {
            if (var_set == "var_set1")
            {
                VectorDynamic x = GetVariables()->GetComponent("var_set1")->GetValues();
                MatrixDynamic jj = NumericalDerivativeDynamic(f, x, deltaOptimizer);

                for (uint i = 0; i < x.rows(); i++)
                    jac_block.coeffRef(0, i) = jj.coeff(0, i);
            }
        }
    };

    template <class TaskSetType, class ExVariablesT, class ExConstraintT, class ExCostT>
    Eigen::VectorXd OptimizeIfopt(TaskSetType &tasks, boost::optional<VectorDynamic> otherParameters = boost::none)
    {
        // 1. define the problem
        ifopt::Problem nlp;
        nlp.AddVariableSet(std::make_shared<ExVariablesT>(tasks.tasks_));
        nlp.AddConstraintSet(std::make_shared<ExConstraintT>(tasks));
        if (otherParameters != boost::none) // this 'if' statement distinguishes control and energy cases
            nlp.AddCostSet(std::make_shared<ExCostT>(tasks.tasks_, otherParameters.get()));
        else
            nlp.AddCostSet(std::make_shared<ExCostT>(tasks.tasks_));
        if (debugMode == 1)
            nlp.PrintCurrent();

        // 2. choose solver and options
        ifopt::IpoptSolver ipopt;
        ipopt.SetOption("linear_solver", "mumps");
        ipopt.SetOption("jacobian_approximation", "exact");
        ipopt.SetOption("max_cpu_time", 600); // time-out after 600 seconds

        // 3 . solve
        ipopt.Solve(nlp);
        Eigen::VectorXd x = nlp.GetOptVariables()->GetValues();

        double y[x.rows()];
        Eigen2Array(x, y);
        if (debugMode == 1)
        {
            std::cout << "Obj: " << nlp.EvaluateCostFunction(y) << std::endl;
            std::cout << "Optimal variable found: " << x << std::endl;
        }
        return x;
    }

}