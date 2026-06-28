# TIN 三角网生成作业

本作业使用 C++ 实现基于 Bowyer-Watson 逐点插入法的 Delaunay TIN 三角网生成。程序固定生成 50 个模拟地形采样点，根据点的平面坐标进行三角剖分，并将高程用于结果着色和三维网格导出。

## 文件说明

- `cppks.cpp`：程序入口，负责生成采样点、调用构网函数并导出结果。
- `tin.h` / `tin.cpp`：TIN 数据结构、Delaunay 构网算法、CSV/SVG/OBJ 导出实现。
- `build_msvc.bat`：MSVC 一键编译脚本，已加入 `/utf-8` 以支持源码中的中文注释。
- `tin_vertices.csv`：顶点编号、x/y 平面坐标和 z 高程。
- `tin_triangles.csv`：三角形编号及其三个顶点下标。
- `tin_result.svg`：二维 TIN 三角网可视化结果。
- `tin_mesh.obj`：可导入三维软件查看的 TIN 网格。
- `061242+李玟+TIN 三角网生成.pdf`：作业报告。

## 编译与运行

在 Visual Studio C++ 开发环境可用的 Windows 机器上执行：

```bat
build_msvc.bat
tin_app.exe
```

程序运行后会在当前目录生成：

```text
tin_vertices.csv
tin_triangles.csv
tin_result.svg
tin_mesh.obj
```

## 本机验证结果

已在本机使用 Visual Studio 2026 C++ 工具链验证通过：

```text
TIN triangulation completed.
Points: 50
Triangles: 88
Edges: 137
Elevation range: 72.12 - 127.00
```

说明：演示视频 `061242+李玟+TIN三角网生成.mp4` 大小约 145 MB，超过 GitHub 普通 Git 单文件 100 MB 限制，未作为默认上传文件；如需上传视频，建议使用 Git LFS 或发布到 Release。
