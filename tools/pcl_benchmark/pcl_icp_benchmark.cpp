#include <pcl/io/vtk_lib_io.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/registration/gicp.h>
#include <pcl/registration/icp.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

bool loadXYZCsv(const std::string& path, pcl::PointCloud<pcl::PointXYZ>::Ptr cloud) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "[PCL benchmark] cannot open " << path << "\n";
        return false;
    }
    std::string line;
    bool first = true;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (first) {
            first = false;
            if (!std::isdigit(line[0]) && line[0] != '-') continue;
        }
        std::stringstream ss(line);
        std::string token;
        std::vector<float> vals;
        while (std::getline(ss, token, ',')) {
            try { vals.push_back(std::stof(token)); }
            catch (...) { vals.push_back(0.0f); }
        }
        if (vals.size() >= 3) {
            cloud->points.push_back({vals[0], vals[1], vals[2]});
        }
    }
    cloud->width = cloud->points.size();
    cloud->height = 1;
    cloud->is_dense = true;
    return !cloud->points.empty();
}

bool loadStlVertices(const std::string& path, pcl::PointCloud<pcl::PointXYZ>::Ptr cloud) {
    pcl::PolygonMesh mesh;
    if (pcl::io::loadPolygonFileSTL(path, mesh) == 0) {
        std::cerr << "[PCL benchmark] cannot load STL " << path << "\n";
        return false;
    }
    pcl::fromPCLPointCloud2(mesh.cloud, *cloud);
    return !cloud->empty();
}

bool loadAuto(const std::string& path, pcl::PointCloud<pcl::PointXYZ>::Ptr cloud) {
    const auto pos = path.rfind('.');
    if (pos == std::string::npos) return false;
    const std::string ext = path.substr(pos);
    if (ext == ".stl" || ext == ".STL") return loadStlVertices(path, cloud);
    return loadXYZCsv(path, cloud);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "usage: pcl_icp_benchmark <source.csv> <target.csv> <output.csv> "
                     "[max_iters=18] [max_corr_dist=8.0] [algo=icp|gicp]\n";
        return 1;
    }

    const int max_iters = (argc > 4) ? std::atoi(argv[4]) : 18;
    const float max_corr = (argc > 5) ? std::atof(argv[5]) : 8.0f;
    const std::string algo = (argc > 6) ? argv[6] : "icp";

    auto src = pcl::PointCloud<pcl::PointXYZ>::Ptr(new pcl::PointCloud<pcl::PointXYZ>);
    auto tgt = pcl::PointCloud<pcl::PointXYZ>::Ptr(new pcl::PointCloud<pcl::PointXYZ>);
    if (!loadAuto(argv[1], src) || !loadAuto(argv[2], tgt)) return 1;

    std::cout << "[PCL benchmark] algo=" << algo
              << " src=" << src->size()
              << " tgt=" << tgt->size()
              << " max_iters=" << max_iters
              << " max_corr=" << max_corr << "mm\n";

    pcl::PointCloud<pcl::PointXYZ> aligned;
    Eigen::Matrix4f T = Eigen::Matrix4f::Identity();
    int iters = 0;
    double rmse = 0.0;
    bool converged = false;

    auto t0 = std::chrono::high_resolution_clock::now();
    if (algo == "gicp") {
        pcl::GeneralizedIterativeClosestPoint<pcl::PointXYZ, pcl::PointXYZ> gicp;
        gicp.setInputSource(src);
        gicp.setInputTarget(tgt);
        gicp.setMaxCorrespondenceDistance(max_corr);
        gicp.setMaximumIterations(max_iters);
        gicp.setTransformationEpsilon(1e-6);
        gicp.setEuclideanFitnessEpsilon(1e-6);
        gicp.align(aligned);
        T = gicp.getFinalTransformation();
        iters = gicp.nr_iterations_;
        rmse = std::sqrt(gicp.getFitnessScore());
        converged = gicp.hasConverged();
    } else {
        pcl::IterativeClosestPoint<pcl::PointXYZ, pcl::PointXYZ> icp;
        icp.setInputSource(src);
        icp.setInputTarget(tgt);
        icp.setMaxCorrespondenceDistance(max_corr);
        icp.setMaximumIterations(max_iters);
        icp.setTransformationEpsilon(1e-6);
        icp.setEuclideanFitnessEpsilon(1e-6);
        icp.align(aligned);
        T = icp.getFinalTransformation();
        iters = icp.nr_iterations_;
        rmse = std::sqrt(icp.getFitnessScore());
        converged = icp.hasConverged();
    }
    auto t_end = std::chrono::high_resolution_clock::now();

    const double total_ms =
        std::chrono::duration_cast<std::chrono::microseconds>(t_end - t0).count() / 1000.0;

    std::cout << "[PCL benchmark] converged=" << converged
              << " rmse=" << rmse
              << " iters=" << iters
              << " total_ms=" << total_ms << "\n";

    std::ofstream out(argv[3]);
    out << "backend,src_points,tgt_points,iterations,rmse_mm,total_ms,converged\n";
    out << "pcl_" << algo << "_cpu," << src->size() << "," << tgt->size() << ","
        << iters << "," << rmse << "," << total_ms << ","
        << (converged ? "true" : "false") << "\n";

    std::cout << "Final transform:\n" << T << "\n";

    return 0;
}
