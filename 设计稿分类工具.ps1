# 设计稿分类工具
# 用途：帮助查看和分类设计稿文件

$designDir = "D:\dev\ktvlv\ktvlv\build_ninja2\设计稿"
$outputFile = "D:\dev\ktvlv\ktvlv\design_analysis_result.md"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "设计稿分类工具" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 获取所有设计稿文件
$files = Get-ChildItem -Path $designDir -Filter "*.jpg" | Sort-Object Name

Write-Host "找到 $($files.Count) 个设计稿文件" -ForegroundColor Green
Write-Host ""

# 创建分类结果文档
$content = @"
# 设计稿分类结果

> **分析日期**：$(Get-Date -Format "yyyy-MM-dd HH:mm:ss")  
> **设计稿目录**：`$designDir`  
> **文件总数**：$($files.Count) 个JPG文件

## 一、设计稿文件清单

| 序号 | 原文件名 | 文件大小 | 页面类型 | UI特征描述 | 实现状态 |
|------|----------|----------|----------|------------|----------|
"@

$index = 1
foreach ($file in $files) {
    $sizeKB = [math]::Round($file.Length / 1KB, 0)
    $content += "`n| $index | $($file.Name) | $sizeKB KB | **待分类** | 待描述 | 待实现 |"
    $index++
}

$content += @"

## 二、页面类型说明

### 2.1 一级页面（主框架页面）

- **首页Tab** - 默认显示，包含搜索框、分类入口、歌曲列表
- **历史记录Tab** - 显示最近50首播放记录

### 2.2 二级页面（子页面）

- **搜索页面** - 搜索界面（带虚拟键盘）
- **分类浏览页面** - 按分类浏览歌曲
- **排行榜页面** - 显示各类排行榜
- **歌手页面** - 按歌手浏览歌曲
- **已点列表页面** - 显示已点歌曲队列
- **调音页面** - 音量/升降调调节界面（弹窗）
- **设置页面** - 系统设置界面

## 三、分类指南

### 识别首页的特征：
- 顶部菜单栏：首页/历史记录两个Tab
- 搜索框："Q 输入首字母搜索"
- 分类入口：歌手、排行榜、分类、热歌榜
- 歌曲列表：可滚动的歌曲列表
- 底部控制栏：已点、切歌、伴唱、暂停等按钮

### 识别历史记录页的特征：
- 顶部菜单栏：首页/历史记录（历史记录Tab选中）
- 历史记录列表：显示最近播放的歌曲
- 每条记录显示：歌名、歌手、播放时间

### 识别搜索页面的特征：
- 搜索框：输入框
- 虚拟键盘：A-Z字母键盘
- 搜索结果列表：显示搜索结果

### 识别分类/排行榜/歌手页面的特征：
- 标题栏：显示分类名称
- 返回按钮：返回首页
- 列表内容：对应类型的歌曲列表

### 识别已点列表页面的特征：
- 弹窗形式或全屏页面
- 显示已点歌曲队列
- 删除按钮：每个歌曲项有删除按钮

### 识别调音页面的特征：
- 弹窗形式
- 音量滑块
- 升降调滑块

### 识别设置页面的特征：
- 全屏页面
- 各种设置选项
- 返回按钮

## 四、使用说明

1. 打开每个设计稿文件，查看内容
2. 根据页面特征，确定页面类型
3. 更新上表中的"页面类型"和"UI特征描述"列
4. 对比当前实现，更新"实现状态"列（已实现/部分实现/待实现）

## 五、批量重命名建议

完成分类后，可以使用以下命令批量重命名：

```powershell
# 示例：重命名首页相关文件
Rename-Item "微信图片_20251223203420.jpg" "01_首页_主界面.jpg"
Rename-Item "微信图片_20251223203745.jpg" "02_首页_搜索状态.jpg"
# ... 依此类推
```

---

**提示**：请在查看设计稿后，更新本文档的"页面类型"和"UI特征描述"列。
"@

# 保存到文件
$content | Out-File -FilePath $outputFile -Encoding UTF8

Write-Host "分类结果文档已生成：" -ForegroundColor Green
Write-Host "  $outputFile" -ForegroundColor Yellow
Write-Host ""
Write-Host "下一步操作：" -ForegroundColor Cyan
Write-Host "  1. 打开设计稿目录查看图片" -ForegroundColor White
Write-Host "  2. 打开分类结果文档，填写页面类型和特征描述" -ForegroundColor White
Write-Host "  3. 对比当前实现，更新实现状态" -ForegroundColor White
Write-Host ""

# 询问是否打开设计稿目录
$open = Read-Host "是否打开设计稿目录？(Y/N)"
if ($open -eq "Y" -or $open -eq "y") {
    Start-Process explorer.exe -ArgumentList $designDir
}

