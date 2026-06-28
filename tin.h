#pragma once

// tin.h
// TIN 数据结构和公共函数声明。
// 本文件只描述“有哪些数据和操作”，具体实现位于 tin.cpp。

#include <set>
#include <string>
#include <tuple>
#include <vector>

// 三维地形采样点。
// Delaunay 三角剖分仅使用 x、y 进行平面构网，z 用于表达地形高程。
struct Point {
    double x{};  // 平面横坐标
    double y{};  // 平面纵坐标
    double z{};  // 高程
};

// 无向边，用两个顶点在 points 数组中的下标表示。
// 构造时会将较小下标放入 a、较大下标放入 b，保证同一条边只有一种表示。
struct Edge {
    int a{};  // 较小的端点下标
    int b{};  // 较大的端点下标

    Edge() = default;
    Edge(int first, int second);

    // 提供严格弱序，使 Edge 可以作为 std::map 和 std::set 的键。
    bool operator<(const Edge& other) const;
};

// 三角形面片，a、b、c 是三个顶点下标。
// 程序会统一保持三个顶点为逆时针顺序。
struct Triangle {
    int a{};
    int b{};
    int c{};
};

// 三角形外接圆。
struct Circle {
    double x{};      // 圆心横坐标
    double y{};      // 圆心纵坐标
    double r2{};     // 半径平方，避免圆内判断时重复开平方
    bool valid{};    // 三点共线时无法确定有限外接圆，此时为 false
};

// 计算向量 AB 与 AC 的二维叉积。
// 返回值 > 0：A、B、C 逆时针；< 0：顺时针；= 0：三点共线。
double cross(const Point& a, const Point& b, const Point& c);

// 若三角形顶点为顺时针，则交换 b、c，使其变为逆时针。
Triangle makeCounterClockwise(Triangle t, const std::vector<Point>& points);

// 根据三角形三个顶点计算外接圆；三点近似共线时返回无效圆。
Circle circumcircle(const Triangle& t, const std::vector<Point>& points);

// 判断点 p 是否位于三角形 t 的外接圆内部或圆周上。
bool pointInsideCircumcircle(const Point& p, const Triangle& t, const std::vector<Point>& points);

// 根据平面坐标计算模拟地形高程。
double terrainHeight(double x, double y);

// 在 width × height 区域内生成 count 个保持最小间距的地形点。
std::vector<Point> generateTerrainPoints(int count, int width, int height);

// 使用 Bowyer-Watson 逐点插入法生成 Delaunay TIN。
std::vector<Triangle> buildDelaunayTin(const std::vector<Point>& inputPoints);

// 汇总全部三角形的边，并利用 std::set 去除公共边的重复记录。
std::set<Edge> collectEdges(const std::vector<Triangle>& triangles);

// 将高程值映射成 SVG 使用的十六进制颜色字符串。
std::string colorForHeight(double value, double minValue, double maxValue);

// 以下函数分别导出顶点 CSV、三角形 CSV、三维 OBJ 和二维 SVG。
void writeVerticesCsv(const std::vector<Point>& points, const std::string& fileName);
void writeTrianglesCsv(const std::vector<Triangle>& triangles, const std::string& fileName);
void writeObj(const std::vector<Point>& points, const std::vector<Triangle>& triangles, const std::string& fileName);
void writeSvg(const std::vector<Point>& points, const std::vector<Triangle>& triangles,
              const std::string& fileName, int width, int height);
