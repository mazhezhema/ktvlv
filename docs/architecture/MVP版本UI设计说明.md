# KTV应用MVP版本UI设计说明

> **文档版本**：v1.0  
> **相关文档**：详见 [项目架构设计总览.md](./项目架构设计总览.md)  
> **实现文档**：详见 [C++架构设计-预分配内存版本.md](./C++架构设计-预分配内存版本.md)  
> **开源库选型**：详见 [开源库选型指南.md](./开源库选型指南.md)

## 一、顶部菜单栏设计

### 1.1 菜单项（简化版）

**保留的菜单项：**
- **首页** - 默认选中，显示歌曲浏览界面
- **历史记录** - 显示最近播放的50首歌曲
- **VIP会员中心** - 会员功能（查看会员信息、扫码激活等）

**移除的菜单项（第一期暂不做）：**
- ❌ 猜你喜欢
- ❌ 我的收藏
- ❌ 语音点歌（未来功能）
- ❌ 唱歌打分（未来功能）

### 1.2 UI布局

```
┌─────────────────────────────────────────────────────────┐
│  顶部菜单栏（高度：50px，固定）                           │
│  ┌─────────┐  ┌──────────┐                              │
│  │  首页   │  │ 历史记录 │  ← Tab切换，当前选中高亮显示   │
│  └─────────┘  └──────────┘                              │
├─────────────────────────────────────────────────────────┤
│                                                           │
│  主内容区（根据Tab切换显示不同内容）                       │
│                                                           │
│  [首页Tab内容]                                             │
│  - 歌曲分类/排行榜入口                                    │
│  - 歌曲列表（可滚动）                                      │
│  - 搜索框 + 虚拟键盘                                       │
│                                                           │
│  或                                                       │
│                                                           │
│  [历史记录Tab内容]                                         │
│  - 显示最近50首播放记录（按时间倒序，最新的在上）          │
│  - 每条记录显示：歌名、歌手、播放时间                      │
│  - 点击记录可重新播放                                      │
│                                                           │
├─────────────────────────────────────────────────────────┤
│  底部控制栏（高度：80px，固定）                           │
│  [已点] [切歌] [伴唱] [暂停] [重唱] [调音] [设置] [返回]  │
└─────────────────────────────────────────────────────────┘
```

## 二、历史记录功能详细设计

### 2.1 功能需求

1. **自动记录**：每次播放歌曲时，自动添加到历史记录
2. **数量限制**：最多保存50首，采用FIFO（先进先出）策略
3. **显示顺序**：按播放时间倒序显示（最新播放的在最上面）
4. **持久化**：历史记录保存到本地文件，应用重启后自动加载
5. **快速播放**：点击历史记录中的歌曲，可直接播放

### 2.2 数据结构设计

```c
// 历史记录项
typedef struct {
    char song_id[64];        // 歌曲ID
    char song_name[128];     // 歌曲名称
    char artist[128];        // 歌手名称
    char m3u8_url[256];     // m3u8播放地址
    time_t play_time;        // 播放时间戳
    int play_count;          // 播放次数（可选）
} HistoryItem;

// 历史记录队列（FIFO，最多50首）
typedef struct {
    HistoryItem items[MAX_HISTORY_COUNT];  // MAX_HISTORY_COUNT = 50
    int count;      // 当前记录数量（0-50）
    int head;       // 队列头部索引（用于FIFO覆盖）
    int tail;       // 队列尾部索引
} HistoryQueue;

// 全局历史记录实例
static HistoryQueue g_history_queue = {0};
```

### 2.3 核心功能实现

#### 2.3.1 添加播放记录

```c
/**
 * 添加歌曲到历史记录
 * @param song_id 歌曲ID
 * @param song_name 歌曲名称
 * @param artist 歌手名称
 * @param m3u8_url m3u8播放地址
 */
void history_add_record(const char* song_id, const char* song_name, 
                        const char* artist, const char* m3u8_url) {
    HistoryQueue* queue = &g_history_queue;
    HistoryItem* item;
    
    // 检查是否已存在（避免重复记录）
    int existing_index = history_find_by_id(song_id);
    if (existing_index >= 0) {
        // 已存在，更新播放时间和播放次数
        item = &queue->items[existing_index];
        item->play_time = time(NULL);
        item->play_count++;
        // 移动到队列头部（最新）
        history_move_to_top(existing_index);
        return;
    }
    
    if (queue->count < MAX_HISTORY_COUNT) {
        // 队列未满，添加到末尾
        item = &queue->items[queue->count];
        queue->count++;
    } else {
        // 队列已满，覆盖最旧的记录（FIFO）
        item = &queue->items[queue->head];
        queue->head = (queue->head + 1) % MAX_HISTORY_COUNT;
    }
    
    // 填充记录信息
    strncpy(item->song_id, song_id, sizeof(item->song_id) - 1);
    strncpy(item->song_name, song_name, sizeof(item->song_name) - 1);
    strncpy(item->artist, artist, sizeof(item->artist) - 1);
    strncpy(item->m3u8_url, m3u8_url, sizeof(item->m3u8_url) - 1);
    item->play_time = time(NULL);
    item->play_count = 1;
    
    // 保存到文件
    history_save_to_file();
}
```

#### 2.3.2 获取历史记录列表（按时间倒序）

```c
/**
 * 获取历史记录列表（按播放时间倒序）
 * @param items 输出数组
 * @param max_count 最大数量
 * @return 实际返回的记录数
 */
int history_get_list(HistoryItem* items, int max_count) {
    HistoryQueue* queue = &g_history_queue;
    int count = (queue->count < max_count) ? queue->count : max_count;
    
    // 按时间倒序排序（最新的在前）
    HistoryItem sorted_items[MAX_HISTORY_COUNT];
    memcpy(sorted_items, queue->items, sizeof(HistoryItem) * queue->count);
    qsort(sorted_items, queue->count, sizeof(HistoryItem), history_compare_time);
    
    // 复制到输出数组
    for (int i = 0; i < count; i++) {
        memcpy(&items[i], &sorted_items[i], sizeof(HistoryItem));
    }
    
    return count;
}

// 时间比较函数（用于排序）
static int history_compare_time(const void* a, const void* b) {
    const HistoryItem* item_a = (const HistoryItem*)a;
    const HistoryItem* item_b = (const HistoryItem*)b;
    // 倒序：时间大的在前
    return (int)(item_b->play_time - item_a->play_time);
}
```

#### 2.3.3 持久化存储

```c
// 历史记录文件路径
#define HISTORY_FILE_PATH "/data/ktv_history.json"

/**
 * 保存历史记录到文件（JSON格式）
 */
void history_save_to_file(void) {
    HistoryQueue* queue = &g_history_queue;
    FILE* fp = fopen(HISTORY_FILE_PATH, "w");
    if (!fp) {
        printf("Failed to open history file for writing\n");
        return;
    }
    
    fprintf(fp, "{\n");
    fprintf(fp, "  \"count\": %d,\n", queue->count);
    fprintf(fp, "  \"items\": [\n");
    
    for (int i = 0; i < queue->count; i++) {
        HistoryItem* item = &queue->items[i];
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"song_id\": \"%s\",\n", item->song_id);
        fprintf(fp, "      \"song_name\": \"%s\",\n", item->song_name);
        fprintf(fp, "      \"artist\": \"%s\",\n", item->artist);
        fprintf(fp, "      \"m3u8_url\": \"%s\",\n", item->m3u8_url);
        fprintf(fp, "      \"play_time\": %ld,\n", item->play_time);
        fprintf(fp, "      \"play_count\": %d\n", item->play_count);
        fprintf(fp, "    }%s\n", (i < queue->count - 1) ? "," : "");
    }
    
    fprintf(fp, "  ]\n");
    fprintf(fp, "}\n");
    
    fclose(fp);
}

/**
 * 从文件加载历史记录
 */
void history_load_from_file(void) {
    HistoryQueue* queue = &g_history_queue;
    FILE* fp = fopen(HISTORY_FILE_PATH, "r");
    if (!fp) {
        printf("History file not found, starting with empty history\n");
        return;
    }
    
    // 使用cJSON库（开源库，详见：开源库选型指南.md）解析文件
    // 低代码实现，避免自己实现JSON解析器
    // ...
    
    fclose(fp);
}
```

### 2.4 LVGL界面实现

```c
/**
 * 创建历史记录界面
 */
lv_obj_t* create_history_tab(lv_obj_t* parent) {
    // 创建列表容器
    lv_obj_t* list = lv_list_create(parent);
    lv_obj_set_size(list, LV_PCT(100), LV_PCT(100));
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 0);
    
    // 获取历史记录
    HistoryItem items[MAX_HISTORY_COUNT];
    int count = history_get_list(items, MAX_HISTORY_COUNT);
    
    // 创建列表项
    for (int i = 0; i < count; i++) {
        HistoryItem* item = &items[i];
        
        // 格式化显示文本
        char text[256];
        char time_str[64];
        struct tm* tm_info = localtime(&item->play_time);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M", tm_info);
        
        snprintf(text, sizeof(text), "%s - %s\n%s", 
                 item->song_name, item->artist, time_str);
        
        // 创建列表项
        lv_obj_t* list_item = lv_list_add_btn(list, LV_SYMBOL_AUDIO, text);
        
        // 设置用户数据（存储m3u8_url，用于点击播放）
        lv_obj_set_user_data(list_item, strdup(item->m3u8_url));
        
        // 添加点击事件
        lv_obj_add_event_cb(list_item, history_item_click_cb, LV_EVENT_CLICKED, NULL);
    }
    
    return list;
}

/**
 * 历史记录项点击回调
 */
static void history_item_click_cb(lv_event_t* e) {
    lv_obj_t* item = lv_event_get_target(e);
    char* m3u8_url = (char*)lv_obj_get_user_data(item);
    
    if (m3u8_url) {
        // 播放选中的歌曲
        play_song_from_url(m3u8_url);
    }
}
```

## 三、首页界面设计

### 3.1 布局结构

```
┌─────────────────────────────────────┐
│  首页内容区                          │
├─────────────────────────────────────┤
│  [搜索框]                            │
│  Q 输入首字母搜索                    │
├─────────────────────────────────────┤
│  [分类入口]                          │
│  [歌手] [排行榜] [分类] [热歌榜]     │
├─────────────────────────────────────┤
│  [歌曲列表]（可滚动）                │
│  - 歌曲1 (HD) - 歌手1                │
│  - 歌曲2 (HD) - 歌手2                │
│  - 歌曲3 (HD) - 歌手3                │
│  ...                                 │
├─────────────────────────────────────┤
│  [虚拟键盘]（搜索时显示）             │
│  A B C D E F G H I J ...            │
└─────────────────────────────────────┘
```

### 3.2 功能说明

- **搜索框**：支持首字母搜索（拼音首字母）
- **分类入口**：快速进入不同分类浏览
- **歌曲列表**：显示歌曲名称、歌手、清晰度标识
- **点歌**：点击歌曲添加到"已点"列表

## 四、底部控制栏设计

### 4.1 按钮布局

```
[已点(6)] [切歌] [伴唱] [暂停] [重唱] [调音] [设置] [返回]
```

### 4.2 按钮功能

- **已点**：显示已点歌曲数量，点击打开已点列表
- **切歌**：切换到下一首
- **伴唱**：切换原唱/伴奏
- **暂停**：暂停/继续播放
- **重唱**：重新播放当前歌曲
- **调音**：打开音量/升降调调节界面
- **设置**：系统设置
- **返回**：返回上一级界面

## 五、数据流程

### 5.1 播放歌曲时的历史记录流程

```
用户点击歌曲
    ↓
添加到"已点"列表
    ↓
开始播放（TPlayerStart）
    ↓
播放开始回调触发
    ↓
调用 history_add_record() 添加到历史记录
    ↓
如果历史记录已满50首，删除最旧的记录（FIFO）
    ↓
保存到本地文件（history_save_to_file）
    ↓
如果当前在"历史记录"Tab，刷新列表显示
```

### 5.2 应用启动时的历史记录加载

```
应用启动
    ↓
调用 history_load_from_file()
    ↓
从JSON文件加载历史记录到内存
    ↓
如果切换到"历史记录"Tab，显示加载的记录
```

## 六、实现优先级

### Phase 1（核心功能）
1. ✅ 顶部菜单栏（首页/历史记录两个Tab）
2. ✅ 首页歌曲列表和搜索
3. ✅ 历史记录基本显示（内存中，不持久化）
4. ✅ 播放时自动添加到历史记录

### Phase 2（完善功能）
1. ✅ 历史记录持久化（保存到文件）
2. ✅ 历史记录点击播放
3. ✅ 历史记录去重（同一首歌只保留最新记录）
4. ✅ 历史记录排序（按时间倒序）

### Phase 3（优化）
1. ✅ 历史记录界面优化（显示播放时间、播放次数等）
2. ✅ 历史记录删除功能（可选）
3. ✅ 历史记录清空功能（可选）

## 七、技术要点

1. **FIFO队列实现**：使用循环数组实现，当队列满时覆盖最旧的记录
2. **去重逻辑**：添加记录前检查是否已存在，存在则更新时间和次数，并移动到队列头部
3. **持久化格式**：使用JSON格式，便于解析和调试
4. **线程安全**：如果历史记录操作在多线程环境，需要加锁保护
5. **内存管理**：注意字符串拷贝的安全性和内存释放

---

**总结**：MVP版本简化了顶部菜单栏，只保留"首页"和"历史记录"两个Tab，历史记录功能限制为最多50首，采用FIFO策略自动管理，并支持持久化存储。

