// cppks.cpp
// 程序入口文件：负责设置运行参数、调用 TIN 构网函数、导出结果并打印统计信息。
// 具体的计算几何算法和文件输出实现在 tin.cpp 中。

#include "tin.h"

#include <algorithm>
#include <exception>
#include <iomanip>
#include <iostream>

int main()
{
    // SVG 画布大小，同时也是随机采样点所在二维区域的大小。
    constexpr int width = 640;
    constexpr int height = 480;

    // 本次实验固定生成 50 个地形采样点。
    // generateTerrainPoints() 内部使用固定随机种子，因此每次运行结果相同。
    constexpr int pointCount = 50;

    try {
        // 第一步：生成带有 (x, y, z) 坐标的模拟地形采样点。
        const std::vector<Point> points = generateTerrainPoints(pointCount, width, height);

        // 第二步：使用 Bowyer-Watson 逐点插入算法生成 Delaunay TIN。
        // triangles 中保存的是顶点下标，而不是重复保存顶点坐标。
        const std::vector<Triangle> triangles = buildDelaunayTin(points);

        // 第三步：从全部三角形中提取并去重无向边，用于统计网格边数。
        const std::set<Edge> edges = collectEdges(triangles);

        // 第四步：将同一份 TIN 数据导出为多种通用格式。
        // CSV 用于检查数据，SVG 用于二维查看，OBJ 用于三维软件查看。
        writeVerticesCsv(points, "tin_vertices.csv");
        writeTrianglesCsv(triangles, "tin_triangles.csv");
        writeSvg(points, triangles, "tin_result.svg", width, height);
        writeObj(points, triangles, "tin_mesh.obj");

        // 一次遍历同时找到最低点和最高点，便于输出本次地形的高程范围。
        const auto [minIt, maxIt] = std::minmax_element(points.begin(), points.end(), [](const Point& a, const Point& b) {
            return a.z < b.z;
        });

        // 输出本次构网的关键统计数据和生成文件名。
        std::cout << "TIN triangulation completed.\n";
        std::cout << "Points: " << points.size() << '\n';
        std::cout << "Triangles: " << triangles.size() << '\n';
        std::cout << "Edges: " << edges.size() << '\n';
        std::cout << "Elevation range: " << std::fixed << std::setprecision(2)
                  << minIt->z << " - " << maxIt->z << '\n';
        std::cout << "Output files:\n";
        std::cout << "  tin_vertices.csv\n";
        std::cout << "  tin_triangles.csv\n";
        std::cout << "  tin_result.svg\n";
        std::cout << "  tin_mesh.obj\n";
    } catch (const std::exception& ex) {
        // 捕获采样点生成或文件处理过程中抛出的标准异常。
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }

    // 返回 0 表示程序正常完成。
    return 0;
}
