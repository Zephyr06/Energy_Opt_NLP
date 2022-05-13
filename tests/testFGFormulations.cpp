
#include <chrono>

#include <CppUnitLite/TestHarness.h>
#include <gtsam/inference/Symbol.h>
#include <gtsam/nonlinear/NonlinearFactorGraph.h>
#include "gtsam/nonlinear/LevenbergMarquardtOptimizer.h"
#include <gtsam/nonlinear/GaussNewtonOptimizer.h>

typedef Eigen::Matrix<double, Eigen::Dynamic, 1> VectorDynamic;
typedef Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> MatrixDynamic;
typedef long long int LLint;

using namespace std;
using namespace gtsam;
using namespace chrono;

inline VectorDynamic GenerateVectorDynamic(LLint N)
{
    VectorDynamic v;
    v.resize(N, 1);
    v.setZero();
    return v;
}
inline VectorDynamic GenerateVectorDynamic1D(double x)
{
    VectorDynamic res = GenerateVectorDynamic(1);
    res << x;
    return res;
}

inline MatrixDynamic GenerateMatrixDynamic(int m, int n)
{
    MatrixDynamic M;
    M.resize(m, n);
    M.setZero();
    return M;
}

gtsam::Symbol GenerateControlKey(int idtask, std::string type)
{
    if (type == "period")
    {
        return gtsam::Symbol('t', idtask);
    }
    else if (type == "response")
    {
        return gtsam::Symbol('r', idtask);
    }
    else if (type == "executionTime")
    {
        return gtsam::Symbol('c', idtask);
    }
    else
    {
        // CoutError("Unrecognized type in GenerateControlKey");
        gtsam::Symbol key('a', idtask);
        return key;
    }
}

double Fobj(double x)
{
    return x * x * x + sin(x) + cos(3 * x * x);
}

double Jobj(double x)
{
    return 3 * x * x + cos(x) - sin(3 * x * x) * 6 * x;
}
int Nvar = 1;

class MyTestFactor1D : public gtsam::NoiseModelFactor1<VectorDynamic>
{
public:
    MyTestFactor1D(Key key,
                   SharedNoiseModel model) : NoiseModelFactor1<VectorDynamic>(model, key) {}
    Vector evaluateError(const VectorDynamic &x,
                         boost::optional<Matrix &> H = boost::none) const override
    {
        VectorDynamic err = GenerateVectorDynamic1D(Fobj(x(0)));
        if (H)
        {
            *H = GenerateVectorDynamic1D(Jobj(x(0)));
            cout << "Jacobian is " << *H << endl;
            cout << "Error is " << err << endl;
        }
        return err;
    }
};

class MyTestFactorND : public NoiseModelFactor1<VectorDynamic>
{
public:
    MyTestFactorND(Key key,
                   SharedNoiseModel model) : NoiseModelFactor1<VectorDynamic>(model, key) {}
    Vector evaluateError(const VectorDynamic &x,
                         boost::optional<Matrix &> H = boost::none) const override
    {
        // int n = x.rows();
        VectorDynamic err = GenerateVectorDynamic(Nvar);
        for (int i = 0; i < Nvar; i++)
        {
            err(i) = Fobj(x(i));
        }
        if (H)
        {
            MatrixDynamic J = GenerateMatrixDynamic(Nvar, Nvar);
            for (int i = 0; i < Nvar; i++)
            {
                J(i, i) = Jobj(x(i));
            }
            *H = J;
            // *H = GenerateVectorDynamic1D(Jobj(x(0)));
        }
        return err;
    }
};

TEST(multiple_small, v1)
{
    Nvar = 1;
    auto start = high_resolution_clock::now();
    auto model = noiseModel::Isotropic::Sigma(1, 1);
    NonlinearFactorGraph graph;
    Values initialEstimateFG;

    for (int i = 0; i < Nvar; i++)
    {
        graph.emplace_shared<MyTestFactor1D>(GenerateControlKey(i, "period"), model);
        initialEstimateFG.insert(GenerateControlKey(i, "period"), GenerateVectorDynamic1D(5));
    }

    auto sth = graph.linearize(initialEstimateFG)->jacobian();
    MatrixDynamic jacobianCurr = sth.first;
    std::cout << "Current Jacobian matrix:" << std::endl;
    std::cout << jacobianCurr << std::endl;
    std::cout << "Current b vector: " << std::endl;
    std::cout << sth.second << std::endl
              << std::endl;

    Values result;
    // LevenbergMarquardtParams params;
    // params.setlambdaInitial(1e3);
    // params.setVerbosityLM("TRYDELTA");
    // // params.setlambdaLowerBound(lowerLambda);
    // // params.setlambdaUpperBound(upperLambda);
    // // params.setRelativeErrorTol(relativeErrorTolerance);
    // LevenbergMarquardtOptimizer optimizer(graph, initialEstimateFG, params);

    gtsam::GaussNewtonParams params;
    params.setVerbosity("DELTA");
    gtsam::GaussNewtonOptimizer optimizer(graph, initialEstimateFG, params);
    result = optimizer.optimize();
    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(stop - start);
    cout << "multiple_small:" << duration.count() << endl;
}

// TEST(single_big, v1)
// {
//     auto start = high_resolution_clock::now();
//     auto model = noiseModel::Isotropic::Sigma(Nvar, noiseModelSigma);
//     NonlinearFactorGraph graph;
//     Values initialEstimateFG;

//     graph.emplace_shared<MyTestFactorND>(GenerateControlKey(0, "period"), model);
//     initialEstimateFG.insert(GenerateControlKey(0, "period"), GenerateVectorDynamic(Nvar));

//     Values result;
//     LevenbergMarquardtParams params;
//     params.setlambdaInitial(initialLambda);
//     params.setVerbosityLM("SUMMARY");
//     params.setlambdaLowerBound(lowerLambda);
//     params.setlambdaUpperBound(upperLambda);
//     params.setRelativeErrorTol(relativeErrorTolerance);
//     LevenbergMarquardtOptimizer optimizer(graph, initialEstimateFG, params);
//     result = optimizer.optimize();
//     auto stop = high_resolution_clock::now();
//     auto duration = duration_cast<microseconds>(stop - start);
//     cout << "single_big:" << duration.count() << endl;
// }
int main()
{
    TestResult tr;
    return TestRegistry::runAllTests(tr);
}
