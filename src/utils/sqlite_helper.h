// sqlite_helper.h
// SQLite 单例辅助类（进程内唯一 DB）
//
// ============================================================
// 使用原则（贴墙版）：
// 1. 项目启动时 Init
// 2. Exec 用于写（INSERT / UPDATE / DELETE / CREATE TABLE）
// 3. Query 用于读（SELECT）
// 4. Query 返回的都是字符串，由业务层解释
// ============================================================
//
// 示例 1：项目启动
//   SqliteHelper::Init("/data/ktvlv.db");
//   SqliteHelper::Exec("CREATE TABLE IF NOT EXISTS history (id INTEGER PRIMARY KEY, song TEXT)");
//
// 示例 2：插入
//   SqliteHelper::Exec("INSERT INTO history(song) VALUES('稻香')");
//
// 示例 3：查询
//   std::vector<SqlRow> rows;
//   SqliteHelper::Query("SELECT id, song FROM history", rows);
//   for (auto& row : rows) {
//       int id = std::atoi(row.cols[0].c_str());
//       std::string song = row.cols[1];
//   }
//
// 使用边界：
// ✅ 允许：Service 层
// ❌ 禁止：UI 层、Player 层、LVGL callback
// ============================================================
#pragma once

#include <string>
#include <vector>

namespace ktv::utils {

// 一行查询结果：字符串数组
struct SqlRow {
    std::vector<std::string> cols;
};

// SQLite 单例辅助类
class SqliteHelper {
public:
    /**
     * 初始化数据库（项目启动时调用一次）
     * @param db_path 数据库文件路径
     * @return 0 成功；<0 失败
     */
    static int Init(const char* db_path);

    /**
     * 关闭数据库（可选，进程退出时调用）
     */
    static void Shutdown();

    /**
     * 执行无返回 SQL（INSERT / UPDATE / DELETE / CREATE TABLE）
     * @param sql SQL 语句
     * @return 0 成功；<0 失败
     */
    static int Exec(const char* sql);

    /**
     * 查询 SQL，返回多行（每行是字符串数组）
     * @param sql SQL 语句
     * @param rows 输出：查询结果
     * @return 0 成功；<0 失败
     */
    static int Query(const char* sql, std::vector<SqlRow>& rows);

    /**
     * 检查是否已初始化
     */
    static bool IsInitialized();

private:
    SqliteHelper() = delete;
    ~SqliteHelper() = delete;
    SqliteHelper(const SqliteHelper&) = delete;
    SqliteHelper& operator=(const SqliteHelper&) = delete;
};

}  // namespace ktv::utils

