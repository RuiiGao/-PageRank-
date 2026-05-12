@echo off
chcp 65001 > nul

echo =========================================
echo 编译《三国演义》人物关系 PageRank 分析程序
echo =========================================

g++ src/main.cpp -o sanguo_pagerank.exe -std=c++17
if errorlevel 1 (
    echo.
    echo 编译失败，请确认已经安装 g++，并且 g++ 已加入 PATH。
    pause
    exit /b 1
)

echo.
echo 编译成功，开始运行程序...
echo.

sanguo_pagerank.exe
if errorlevel 1 (
    echo.
    echo 程序运行失败，请检查 data/character_edges_simple.csv 是否存在且格式正确。
    pause
    exit /b 1
)

echo.
echo 运行完成。结果文件位于 output 文件夹。
pause
