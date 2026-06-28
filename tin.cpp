// tin.cpp
// TIN 三角网的核心实现文件，主要包括：
// 1. 基础计算几何操作；
// 2. 模拟地形采样点生成；
// 3. Bowyer-Watson Delaunay 三角剖分；
// 4. CSV、SVG、OBJ 结果导出。

#include "tin.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <map>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>

// ---------------------------- 基础几何结构 ----------------------------

Edge::Edge(int first, int second)
{
    // Edge 表示无向边，因此 (2, 5) 和 (5, 2) 应被视为同一条边。
    // 统一端点顺序后，可以直接使用 map 计数或使用 set 去重。
    if (first < second) {
        a = first;
        b = second;
    } else {
        a = second;
        b = first;
    }
}

bool Edge::operator<(const Edge& other) const
{
    // 先比较 a，再比较 b，形成稳定的字典序。
    return std::tie(a, b) < std::tie(other.a, other.b);
}

double cross(const Point& a, const Point& b, const Point& c)
{
    // 二维叉积公式：(B-A) × (C-A)。
    // 叉积符号表示从 AB 转向 AC 时的旋转方向。
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

Triangle makeCounterClockwise(Triangle t, const std::vector<Point>& points)
{
    // OBJ 面片和后续几何计算最好使用一致的顶点绕序。
    // 若叉积为负，说明当前为顺时针，交换后两个顶点即可改为逆时针。
    if (cross(points[t.a], points[t.b], points[t.c]) < 0.0) {
        std::swap(t.b, t.c);
    }
    return t;
}

Circle circumcircle(const Triangle& t, const std::vector<Point>& points)
{
    // 取出三角形的三个顶点。这里只使用 x、y 坐标计算平面外接圆，
    // z 高程不会影响 Delaunay 三角剖分的平面拓扑关系。
    const Point& a = points[t.a];
    const Point& b = points[t.b];
    const Point& c = points[t.c];
    // 外接圆圆心公式的公共分母。
    // d 与三角形有向面积成正比；d 接近 0 表示三点近似共线。
    const double d = 2.0 * (a.x * (b.y - c.y) + b.x * (c.y - a.y) + c.x * (a.y - b.y));
    if (std::abs(d) < 1e-12) {
        // 共线点不存在有限外接圆，返回 valid=false 的默认 Circle。
        return {};
    }

    // 三个顶点到原点的距离平方，是解析求解圆心所需的中间量。
    const double aa = a.x * a.x + a.y * a.y;
    const double bb = b.x * b.x + b.y * b.y;
    const double cc = c.x * c.x + c.y * c.y;
    Circle circle;

    // 由三点坐标直接求外接圆圆心 (circle.x, circle.y)。
    circle.x = (aa * (b.y - c.y) + bb * (c.y - a.y) + cc * (a.y - b.y)) / d;
    circle.y = (aa * (c.x - b.x) + bb * (a.x - c.x) + cc * (b.x - a.x)) / d;
    const double dx = circle.x - a.x;
    const double dy = circle.y - a.y;
    // 保存半径平方而非半径。后续圆内判断同样比较距离平方，
    // 可避免 sqrt()，减少计算量和浮点误差来源。
    circle.r2 = dx * dx + dy * dy;
    circle.valid = true;
    return circle;
}

bool pointInsideCircumcircle(const Point& p, const Triangle& t, const std::vector<Point>& points)
{
    const Circle circle = circumcircle(t, points);
    if (!circle.valid) {
        // 退化三角形没有可用于 Delaunay 判断的外接圆。
        return false;
    }
    const double dx = p.x - circle.x;
    const double dy = p.y - circle.y;
    // 增加 1e-9 容差，将非常接近圆周的点视为圆内/圆上，
    // 降低浮点舍入误差造成的不稳定结果。
    return dx * dx + dy * dy <= circle.r2 + 1e-9;
}

// ---------------------------- 地形数据生成 ----------------------------

double terrainHeight(double x, double y)
{
    // 使用两个高斯山丘、一个高斯谷地和周期波叠加构造模拟地形。
    // 该函数仅负责生成 z 高程，Delaunay 构网仍然只依据 x、y。
    const double hill1 = 42.0 * std::exp(-((x - 280.0) * (x - 280.0) + (y - 180.0) * (y - 180.0)) / 28000.0);
    const double hill2 = 28.0 * std::exp(-((x - 120.0) * (x - 120.0) + (y - 360.0) * (y - 360.0)) / 16000.0);
    const double valley = 24.0 * std::exp(-((x - 450.0) * (x - 450.0) + (y - 330.0) * (y - 330.0)) / 22000.0);
    const double wave = 10.0 * std::sin(x / 62.0) + 7.0 * std::cos(y / 54.0);
    return 80.0 + hill1 + hill2 - valley + wave;
}

std::vector<Point> generateTerrainPoints(int count, int width, int height)
{
    // 固定随机种子保证实验可重复：每次运行生成相同的采样点和三角网。
    std::mt19937 rng(202606151934);

    // 四周保留 35 个单位的边距，避免采样点紧贴 SVG 画布边缘。
    std::uniform_real_distribution<double> xDist(35.0, width - 35.0);
    std::uniform_real_distribution<double> yDist(35.0, height - 35.0);
    std::vector<Point> points;
    points.reserve(count);

    // guard 防止在区域过小或点数过多时无限尝试。
    int guard = 0;
    while (static_cast<int>(points.size()) < count && guard < count * 200) {
        ++guard;
        Point p{ xDist(rng), yDist(rng), 0.0 };

        // 检查候选点与已有点的平面距离，最小间距设为 22。
        // 这里同样比较距离平方，避免进行平方根运算。
        const bool tooClose = std::any_of(points.begin(), points.end(), [&](const Point& q) {
            const double dx = p.x - q.x;
            const double dy = p.y - q.y;
            return dx * dx + dy * dy < 22.0 * 22.0;
        });
        if (!tooClose) {
            // 平面位置通过间距检查后，再计算并保存该点的高程。
            p.z = terrainHeight(p.x, p.y);
            points.push_back(p);
        }
    }
    if (static_cast<int>(points.size()) != count) {
        // 达到最大尝试次数仍未得到足够点时，交由 main() 捕获并报告错误。
        throw std::runtime_error("Could not generate enough well-spaced terrain points.");
    }
    return points;
}

// ---------------------- Bowyer-Watson Delaunay 构网 ----------------------

std::vector<Triangle> buildDelaunayTin(const std::vector<Point>& inputPoints)
{
    // 三角剖分至少需要三个点。
    if (inputPoints.size() < 3) {
        return {};
    }

    // points 是工作副本。后面会临时追加超级三角形的三个辅助顶点，
    // 原始 inputPoints 不会被修改。
    std::vector<Point> points = inputPoints;

    // 计算全部输入点的轴对齐包围盒。
    double minX = inputPoints.front().x;
    double maxX = inputPoints.front().x;
    double minY = inputPoints.front().y;
    double maxY = inputPoints.front().y;
    for (const Point& p : inputPoints) {
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
    }

    // delta 表示包围盒较长的一边，midX、midY 是包围盒中心。
    // 超级三角形相对于 delta 放大 20 倍，确保所有输入点位于其内部。
    const double delta = std::max(maxX - minX, maxY - minY);
    const double midX = (minX + maxX) / 2.0;
    const double midY = (minY + maxY) / 2.0;
    // 记录三个辅助顶点的下标。superA 等于原始点数，
    // 因而下标 >= superA 的点都属于超级三角形。
    const int superA = static_cast<int>(points.size());
    const int superB = superA + 1;
    const int superC = superA + 2;
    points.push_back({ midX - 20.0 * delta, midY - delta, 0.0 });
    points.push_back({ midX, midY + 20.0 * delta, 0.0 });
    points.push_back({ midX + 20.0 * delta, midY - delta, 0.0 });

    // 初始三角网仅包含超级三角形。
    std::vector<Triangle> triangles{ makeCounterClockwise({ superA, superB, superC }, points) };

    // 按输入顺序逐个插入真实采样点。
    for (int pointIndex = 0; pointIndex < static_cast<int>(inputPoints.size()); ++pointIndex) {
        // kept 保存本轮结束后仍然有效的三角形。
        std::vector<Triangle> kept;

        // edgeUseCount 统计全部坏三角形的边出现次数：
        // 出现两次的边是两个坏三角形之间的公共内部边；
        // 仅出现一次的边是删除坏三角形后形成的空腔边界。
        std::map<Edge, int> edgeUseCount;

        // 找出外接圆包含新点的“坏三角形”。新点加入后，
        // 这些三角形违反 Delaunay 空外接圆条件，必须删除。
        for (const Triangle& triangle : triangles) {
            if (pointInsideCircumcircle(points[pointIndex], triangle, points)) {
                // 不把坏三角形放入 kept，只统计其三条边。
                ++edgeUseCount[Edge(triangle.a, triangle.b)];
                ++edgeUseCount[Edge(triangle.b, triangle.c)];
                ++edgeUseCount[Edge(triangle.c, triangle.a)];
            } else {
                // 外接圆不包含新点的三角形不受本次插入影响，直接保留。
                kept.push_back(triangle);
            }
        }

        // 使用新点与每一条空腔边界组成新三角形，完成局部重构。
        for (const auto& [edge, useCount] : edgeUseCount) {
            if (useCount == 1) {
                // 统一为逆时针顶点顺序后加入新的三角形集合。
                kept.push_back(makeCounterClockwise({ edge.a, edge.b, pointIndex }, points));
            }
        }

        // 用本轮重构后的结果替换旧三角网，继续插入下一个点。
        triangles.swap(kept);
    }

    // 所有真实点插入完毕后，删除包含超级三角形辅助顶点的面。
    // 只有三个顶点下标均小于 superA 的三角形才属于最终结果。
    std::vector<Triangle> result;
    for (const Triangle& triangle : triangles) {
        if (triangle.a < superA && triangle.b < superA && triangle.c < superA) {
            result.push_back(makeCounterClockwise(triangle, inputPoints));
        }
    }
    return result;
}

// ---------------------------- 网格统计与导出 ----------------------------

std::set<Edge> collectEdges(const std::vector<Triangle>& triangles)
{
    std::set<Edge> edges;
    for (const Triangle& triangle : triangles) {
        // Edge 会自动规范端点顺序，set 会自动忽略相邻三角形的重复公共边。
        edges.insert(Edge(triangle.a, triangle.b));
        edges.insert(Edge(triangle.b, triangle.c));
        edges.insert(Edge(triangle.c, triangle.a));
    }
    return edges;
}

std::string colorForHeight(double value, double minValue, double maxValue)
{
    // 将高程线性归一化到 [0, 1]。clamp 防止浮点误差造成越界。
    const double t = std::clamp((value - minValue) / (maxValue - minValue), 0.0, 1.0);

    // 低到高依次使用蓝、绿、黄、棕、浅灰白，模拟常见地形设色。
    const std::array<std::array<int, 3>, 5> palette{ { { 48, 105, 152 }, { 60, 149, 92 },
        { 218, 190, 110 }, { 151, 109, 75 }, { 240, 240, 232 } } };

    // 找到 value 所处的相邻两个色标，并计算区间内插值比例 local。
    const double scaled = t * (palette.size() - 1);
    const int index = std::min(static_cast<int>(scaled), static_cast<int>(palette.size()) - 2);
    const double local = scaled - index;
    std::array<int, 3> rgb{};
    for (int i = 0; i < 3; ++i) {
        // 对 R、G、B 三个通道分别进行线性插值。
        rgb[i] = static_cast<int>(std::round(palette[index][i] * (1.0 - local) + palette[index + 1][i] * local));
    }

    // 将 RGB 整数转换成 SVG 需要的 #RRGGBB 格式。
    std::ostringstream out;
    out << '#' << std::hex << std::uppercase << std::setfill('0')
        << std::setw(2) << rgb[0] << std::setw(2) << rgb[1] << std::setw(2) << rgb[2];
    return out.str();
}

void writeVerticesCsv(const std::vector<Point>& points, const std::string& fileName)
{
    std::ofstream file(fileName);

    // 每一行保存：顶点编号、平面横坐标、平面纵坐标、高程。
    file << "id,x,y,z\n" << std::fixed << std::setprecision(3);
    for (int i = 0; i < static_cast<int>(points.size()); ++i) {
        file << i << ',' << points[i].x << ',' << points[i].y << ',' << points[i].z << '\n';
    }
}

void writeTrianglesCsv(const std::vector<Triangle>& triangles, const std::string& fileName)
{
    std::ofstream file(fileName);

    // a、b、c 对应 tin_vertices.csv 中的三个顶点编号。
    file << "id,a,b,c\n";
    for (int i = 0; i < static_cast<int>(triangles.size()); ++i) {
        file << i << ',' << triangles[i].a << ',' << triangles[i].b << ',' << triangles[i].c << '\n';
    }
}

void writeObj(const std::vector<Point>& points, const std::vector<Triangle>& triangles, const std::string& fileName)
{
    std::ofstream file(fileName);
    file << "# TIN terrain mesh generated by cppks\n" << std::fixed << std::setprecision(3);

    // OBJ 使用 v 声明顶点。本项目将高程 z 放到 OBJ 的第二坐标，
    // 即输出为 (x, z, y)，使常见三维软件中的 Y 轴表现为竖直方向。
    for (const Point& p : points) {
        file << "v " << p.x << ' ' << p.z << ' ' << p.y << '\n';
    }

    // OBJ 面索引从 1 开始，而 C++ vector 下标从 0 开始，因此统一加 1。
    for (const Triangle& triangle : triangles) {
        file << "f " << triangle.a + 1 << ' ' << triangle.b + 1 << ' ' << triangle.c + 1 << '\n';
    }
}

void writeSvg(const std::vector<Point>& points, const std::vector<Triangle>& triangles,
              const std::string& fileName, int width, int height)
{
    // 计算高程范围，用于把不同高程映射到统一色带。
    const auto [minIt, maxIt] = std::minmax_element(points.begin(), points.end(), [](const Point& a, const Point& b) {
        return a.z < b.z;
    });
    const double minZ = minIt->z;
    const double maxZ = maxIt->z;
    std::ofstream file(fileName);

    // 写入 SVG 根节点和浅色背景。viewBox 与采样区域尺寸一致，
    // 因此采样点的 x、y 可以直接作为屏幕坐标使用。
    file << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width << "\" height=\"" << height
         << "\" viewBox=\"0 0 " << width << ' ' << height << "\">\n";
    file << "<rect width=\"100%\" height=\"100%\" fill=\"#F7F7F2\"/>\n" << std::fixed << std::setprecision(2);
    for (const Triangle& t : triangles) {
        // 用三个顶点的平均高程决定整个三角面的填充色。
        const double avgZ = (points[t.a].z + points[t.b].z + points[t.c].z) / 3.0;
        file << "<polygon points=\"" << points[t.a].x << ',' << points[t.a].y << ' '
             << points[t.b].x << ',' << points[t.b].y << ' ' << points[t.c].x << ',' << points[t.c].y
             << "\" fill=\"" << colorForHeight(avgZ, minZ, maxZ)
             << "\" stroke=\"#23303A\" stroke-width=\"0.8\" opacity=\"0.9\"/>\n";
    }

    // 最后绘制黑色采样点，使节点覆盖在三角面和边线之上。
    for (const Point& point : points) {
        file << "<circle cx=\"" << point.x << "\" cy=\"" << point.y << "\" r=\"3\" fill=\"#111111\"/>\n";
    }
    file << "</svg>\n";
}
