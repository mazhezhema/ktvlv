项目网络接口清单

说明：以下多数接口以 `BuildConfig.apiUrl` 作为基地址拼接；标注了常见请求方法与出现位置（示例文件/行）。

- 基础相关（BuildConfig.apiUrl 开头）
  - /karaoke_v3/lic?company=...&app_name=...  (GET)
    - 出处: [app/src/main/java/com/thunder/ktvplayer/LicenseRepository.java](app/src/main/java/com/thunder/ktvplayer/LicenseRepository.java#L117)
  - /karaoke_sdk/vod_token_by_macid?license=...  (GET)  
    - 在重试逻辑中被检测: [app/src/main/java/com/thunder/ktvplayer/utils/NetworkRequest.java](app/src/main/java/com/thunder/ktvplayer/utils/NetworkRequest.java#L209)
  - /karaoke_sdk/vod_token  (GET)
    - 在重试逻辑中被检测: [app/src/main/java/com/thunder/ktvplayer/utils/NetworkRequest.java](app/src/main/java/com/thunder/ktvplayer/utils/NetworkRequest.java#L209)
  - /karaoke_sdk/vod_conf?platform=4.4&token=...&company=...&app_name=...&vn=...  (GET)
    - 出处: [app/src/main/java/com/thunder/ktvplayer/LicenseRepository.java](app/src/main/java/com/thunder/ktvplayer/LicenseRepository.java#L202)
  - /karaoke_sdk/vod_update?platform=4.4&token=...&vn=...&license=...&company=...&app_name=...  (GET)
    - 出处: [app/src/main/java/com/thunder/ktvplayer/LicenseRepository.java](app/src/main/java/com/thunder/ktvplayer/LicenseRepository.java#L228)
    - 其它调用: [app/src/main/java/com/thunder/ktvplayer/LicenseInit.java](app/src/main/java/com/thunder/ktvplayer/LicenseInit.java#L340)
  - /karaoke_sdk/t/plist/set?token=...  (POST)
    - 出处: [app/src/main/java/com/thunder/ktvplayer/Video/VideoPlayerManager.java](app/src/main/java/com/thunder/ktvplayer/Video/VideoPlayerManager.java#L554)
  - /karaoke_sdk/t/like/set?option=...&token=...  (POST)
    - 出处: [app/src/main/java/com/thunder/ktvplayer/Video/VideoPlayer.java](app/src/main/java/com/thunder/ktvplayer/Video/VideoPlayer.java#L813)
    - 选项 0/1/2 在多个位置出现: [app/src/main/java/com/thunder/ktvplayer/LicenseInit.java](app/src/main/java/com/thunder/ktvplayer/LicenseInit.java#L401)
  - /karaoke_sdk/log/upload  (POST multipart)
    - 出处: [app/src/main/java/com/thunder/ktvplayer/utils/QiniuUploader.java](app/src/main/java/com/thunder/ktvplayer/utils/QiniuUploader.java#L245)
  - /karaoke_yiban/token?yiban_status=...&custId=...&macid=...&company=...&app_name=...  (GET)
    - 出处: [app/src/main/java/com/thunder/ktvplayer/LicenseInit.java](app/src/main/java/com/thunder/ktvplayer/LicenseInit.java#L293)
  - /karaoke_sdk/t/yinjixie?license=...&company=...&app_name=...&macid=...&yinjixie=...&expireData=...  (GET)
    - 出处: [app/src/main/java/com/thunder/ktvplayer/LicenseInit.java](app/src/main/java/com/thunder/ktvplayer/LicenseInit.java#L578)
  - /karaoke_v3/mac_key/check?key=...&macid=...&company=...&app_name=...  (GET)
    - 出处: [app/src/main/java/com/thunder/ktvplayer/LicenseFragment.java](app/src/main/java/com/thunder/ktvplayer/LicenseFragment.java#L132)

- 歌曲/歌手/模块相关（BuildConfig.apiUrl 开头）
  - /kcloud/getmusics?token=...&page=...&size=10&word=...&language=...&company=...&app_name=...  (GET)
    - 出处: [app/src/main/java/com/thunder/ktvplayer/SongList.java](app/src/main/java/com/thunder/ktvplayer/SongList.java#L315)
  - /apollo/search/actorsong?token=...&page=...&size=10&key=...&unionid=&actorid=...&company=...&app_name=...  (GET)
    - 出处: [app/src/main/java/com/thunder/ktvplayer/SongList.java](app/src/main/java/com/thunder/ktvplayer/SongList.java#L317)
  - /apollo/module/songlist?token=...&page=...&size=...&id=...&unionid=...&company=...&app_name=...  (GET)
    - 出处: [app/src/main/java/com/thunder/ktvplayer/SongList.java](app/src/main/java/com/thunder/ktvplayer/SongList.java#L320)
    - 亦出现在: [app/src/main/java/com/thunder/ktvplayer/Video/VideoPlayerManager.java](app/src/main/java/com/thunder/ktvplayer/Video/VideoPlayerManager.java#L821)
  - /vodc/song/list?token=...&p=...&tp=...&size=...&type=...&openid=&unionid=...&company=...&app_name=...  (GET)
    - 出处: [app/src/main/java/com/thunder/ktvplayer/SongList.java](app/src/main/java/com/thunder/ktvplayer/SongList.java#L325)
  - /vodc/usersong/analyse?token=...&company=...&app_name=...&song_ids=...  (GET)
    - 出处: [app/src/main/java/com/thunder/ktvplayer/SongList.java](app/src/main/java/com/thunder/ktvplayer/SongList.java#L348)
  - /kcloud/getsingerlist_new?token=...&page=...&size=...&word=...&tp=...&company=...&app_name=...  (GET)
    - 出处: [app/src/main/java/com/thunder/ktvplayer/SingerList.java](app/src/main/java/com/thunder/ktvplayer/SingerList.java#L224)
  - /apollo/module/modulelist?id=2  (GET)
    - 出处: [app/src/main/java/com/thunder/ktvplayer/SecondFragment.java](app/src/main/java/com/thunder/ktvplayer/SecondFragment.java#L241)
  - /vodc/song/topics  (GET)
    - 出处: [app/src/main/java/com/thunder/ktvplayer/SecondFragment.java](app/src/main/java/com/thunder/ktvplayer/SecondFragment.java#L277)

- 其它 BuildConfig.apiUrl 拼接接口
  - /MusicService.aspx?op=getmusicinfobynos&depot=2&nos=...  (GET)
    - 出处: [app/src/main/java/com/thunder/ktvplayer/utils/SongDownloadManager.java](app/src/main/java/com/thunder/ktvplayer/utils/SongDownloadManager.java#L115)
  - /pub?id=...  (DELETE)
    - 出处: [app/src/main/java/com/thunder/ktvplayer/utils/WebSocketClient.java](app/src/main/java/com/thunder/ktvplayer/utils/WebSocketClient.java#L76)

- 绝对/第三方 URL（未使用 BuildConfig.apiUrl）
  - https://qnsign.ktvsky.com/qn/sign/v2?file_name=other/...&bucket=common-web  (GET)
    - 出处: [app/src/main/java/com/thunder/ktvplayer/utils/QiniuUploader.java](app/src/main/java/com/thunder/ktvplayer/utils/QiniuUploader.java#L268)
  - wss://mc.ktv.com.cn/ws/{license}  (WebSocket)
    - 出处: [app/src/main/java/com/thunder/ktvplayer/utils/WebSocketClient.java](app/src/main/java/com/thunder/ktvplayer/utils/WebSocketClient.java#L1)
  - https://mc.ktv.com.cn/yjx/pay?license=...&company=...&app_name=...&macid=...  (用于二维码)
    - 出处: [app/src/main/java/com/thunder/ktvplayer/Video/FullscreenView.java](app/src/main/java/com/thunder/ktvplayer/Video/FullscreenView.java#L387)
    - 亦出现在: [app/src/main/java/com/thunder/ktvplayer/MyPresentation.java](app/src/main/java/com/thunder/ktvplayer/MyPresentation.java#L199)
  - https://mcfd.ktv.com.cn/mobile_music/share?score=... (GET)
    - 出处: [app/src/main/java/com/thunder/ktvplayer/dialog/ScoreResultPopup.java](app/src/main/java/com/thunder/ktvplayer/dialog/ScoreResultPopup.java#L155)

- 说明与下一步建议
  - 以上为代码中直接可见的接口字符串与常用拼接路径（含 GET/POST/DELETE/WS）。
  - 如果你希望我：
    - 生成 CSV/JSON 供自动化使用（可直接导出含 method、path、示例完整 URL），或
    - 在代码中统一抽取并集中管理为一个 `ApiConstants` 类，
    请回复我需要的格式，我会继续修改/生成对应文件。

---
## 接口参数与类型（推断）

以下为对上文列出各接口的查询/路径/表单参数及其推断的数据类型。类型为常见约定；若需精确类型请告知接口文档或代码调用处以进一步校验。

- /karaoke_v3/lic
  - company: string
  - app_name: string

- /karaoke_sdk/vod_token_by_macid
  - license: string

- /karaoke_sdk/vod_token
  - （无查询参数）

- /karaoke_sdk/vod_conf
  - platform: string
  - token: string
  - company: string
  - app_name: string
  - vn: string

- /karaoke_sdk/vod_update
  - platform: string
  - token: string
  - vn: string
  - license: string
  - company: string
  - app_name: string

- /karaoke_sdk/t/plist/set (POST)
  - token: string
  - 其它表单字段: string（视实现）

- /karaoke_sdk/t/like/set (POST)
  - option: integer (enum: 0/1/2)
  - token: string

- /karaoke_sdk/log/upload (POST multipart)
  - file: binary (multipart file)
  - 其它字段: string（视实现）

- /karaoke_yiban/token
  - yiban_status: string
  - custId: string
  - macid: string
  - company: string
  - app_name: string

- /karaoke_sdk/t/yinjixie
  - license: string
  - company: string
  - app_name: string
  - macid: string
  - yinjixie: string 或 boolean
  - expireData: string（日期/时间）

- /karaoke_v3/mac_key/check
  - key: string
  - macid: string
  - company: string
  - app_name: string

- /kcloud/getmusics
  - token: string
  - page: integer
  - size: integer
  - word: string
  - language: string
  - company: string
  - app_name: string

- /apollo/search/actorsong
  - token: string
  - page: integer
  - size: integer
  - key: string
  - unionid: string (optional)
  - actorid: integer 或 string
  - company: string
  - app_name: string

- /apollo/module/songlist
  - token: string
  - page: integer
  - size: integer
  - id: integer
  - unionid: string
  - company: string
  - app_name: string

- /vodc/song/list
  - token: string
  - p: integer
  - tp: integer 或 string
  - size: integer
  - type: integer 或 string
  - openid: string (optional)
  - unionid: string
  - company: string
  - app_name: string

- /vodc/usersong/analyse
  - token: string
  - company: string
  - app_name: string
  - song_ids: string（逗号分隔 ID 列表）

- /kcloud/getsingerlist_new
  - token: string
  - page: integer
  - size: integer
  - word: string
  - tp: integer 或 string
  - company: string
  - app_name: string

- /apollo/module/modulelist
  - id: integer

- /vodc/song/topics
  - （无查询参数）

- /MusicService.aspx?op=getmusicinfobynos
  - op: string
  - depot: integer
  - nos: string（逗号分隔编号）

- /pub (DELETE)
  - id: integer 或 string

- https://qnsign.ktvsky.com/qn/sign/v2
  - file_name: string
  - bucket: string

- wss://mc.ktv.com.cn/ws/{license}
  - license: string (path 参数)

- https://mc.ktv.com.cn/yjx/pay
  - license: string
  - company: string
  - app_name: string
  - macid: string

- https://mcfd.ktv.com.cn/mobile_music/share
  - score: number (int/float)

Base URL： 'https://mc.ktv.com.cn'
company: 渠道号
app_name: 区分不同设备
platform: android 版本
vn: 版本号