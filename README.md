# MHGU 猎人遮挡轮廓

当前可安装产物是 **MHGU-Outline-Trace**。该包在场景深度、猎人及大型怪物独立深度都就绪时，在交换链图像上合成青色轮廓。已在 Astris 任务中观察到大型怪物遮住猎人时出现轮廓，猎人重新露出后轮廓消失。目标平台为 macOS Astris 1.0.32、Switch 国际版 MHGU 1.4.0，Title ID 为 `0100770008DD8000`。

## 调整线宽

解压安装包到游戏的 LayeredFS Mods 目录，在 **Manage Mods** 中启用主模块 **MHGU-Outline-Trace**。在 Astris 游戏库右键 MHGU → **Manage Cheats…**，展开路径含有 `/MHGU-Outline-Trace/cheats/fb08f1d20fd1204f.txt` 的分组。勾选一个「猎人描边线宽」子项：**1 px（细）、3 px（标准）、5 px（粗）、8 px（特粗）**。切换时取消旧档，点 **Done** 后重新启动游戏。

未选或多选时使用 `MHGU-Outline-Trace/romfs/mhgu-outline/outline-width.txt` 中的值（1–8，默认 3）。首次合成时日志会输出实际线宽，如 `MHGU outline width 5 px`。旧版的四个独立线宽 Mod 可以移除。

## 调整任务视距

[下载任务视距 Mod](dist/MHGU-Quest-Camera.zip)，解压到 MHGU 的 Mods 目录。关闭旧的「视距增加」档位，在 **Manage Mods** 启用 **MHGU-Quest-Camera**。打开 **Manage Cheats**，展开路径含有 `/MHGU-Quest-Camera/cheats/` 的分组，只勾选一个任务视距档位（+20%、+40%、+60% 或 +80%）。点击 **Done** 并重新启动游戏。

这些档位对应旧版四个 Mod 的任务镜头距离。未选或同时选中多档时，游戏使用任务镜头资源中的原值。村庄镜头沿用原设置。视距和猎人描边可以独立启用。用自己的 MHGU 国际版 1.4.0 ExeFS 构建视距包：

```sh
python3 tools/build_quest_camera.py /path/to/exefs
```

## 构建与诊断

准备 Python 3、clang、liblz4、nm 和 ld.lld（也可使用 Rust 工具链附带的 rust-lld）。从拥有的游戏导出 1.4.0 原版 ExeFS 后运行

```sh
python3 tools/build_mod.py /path/to/exefs \
  --companion /path/to/MHGU-Damage-Display.zip \
  --companion /path/to/MHGU-Sharpness-Gauge.zip
```

默认包启用七处绘制与合成钩子。需要下文的模型及纹理探针日志时，构建命令加上 `--diagnostics`。两个 companion 参数可省略。脚本检查原版 main、subsdk0 身份及代码哈希，核对十五条原始 ARM 调用，将探针放在 subsdk3。每个调用兼容前置模块映射大小为 0、0x5000、0x8000 或 0xD000 的布局。校验 companion 时检查模块占位、映射大小及与探针的 IPS 写入冲突。

解压 dist/MHGU-Outline-Trace.zip 到该游戏的 LayeredFS Mods 目录，在 Manage Mods 中启用 MHGU-Outline-Trace 并关闭 Forge。重启进入任务后，在 Astris 的 Ryujinx 日志中搜索以下记录。

- `MHGU outline trace initialized` 表示模块初始化。
- `MHGU native outline pass` 后面是实际执行过的原版函数 RVA。每条分支在首次命中时记录，从探针返回后继续执行原函数。若只看到初始化记录，说明观察期间尚未捕获这些分支。
- `MHGU local model slot S part P` 表示在模型更新路径捕获了当前玩家。P 为 0–8 时，对象同时匹配猎人的装备槽指针和父对象；9 表示猎人对象本身。末尾的 update 表示模型更新，draw 表示在网格绘制路径设置材质，每类分别记录首次命中。
- 诊断结束后停用 MHGU-Outline-Trace，重启游戏。

## 已核查的渲染入口

以 main 的 .text 为基准，原版调度函数 0x8D9F9C 根据实例偏移 0x3C 的值选择分支。1 调用 0x8D99AC，2 调用 0x8D9AB4，3 到 5 调用 0x8D76D8。已在本机原版 main 核对三处调用指令。命中本身不能说明输入纹理含有被遮挡的猎人。

本地玩家由会话槽解析。原版 0x28857C 从猎人偏移 0x2384 读取部件槽 0–8，0x890754 返回模型实例偏移 0xF0 的资源指针。探针在模型更新调用点 0x94E258 和 0x94E474 读取当前实例并与槽位交叉检查。另外三处绘制调用 0x8938DC、0x93C780、0x99D174 将当前实例、渲染上下文和网格项传入 0x8926D4。蒙皮路径随后在 0x93C9C8 调用 0x893CA8，将绘制命令加入队列。要确认猎人走过该路径，需看到对应的 draw 记录。

## 遮挡判定与验证进度

`src/silhouette.h` 定义了独立的逐像素判定。输入是同一视口中合并后的猎人覆盖、猎人与前景深度，以及前景是否为大型怪物。先从猎人覆盖求内侧外缘，再按前景归属和深度裁剪。该判定尚待接入游戏实际模型和深度数据。

```sh
clang -std=c11 -Wall -Wextra -Werror tests/silhouette.c -o /tmp/mhgu-silhouette-test
/tmp/mhgu-silhouette-test
```

已通过本机构建、两个 HUD 正式包的布局与补丁比对，以及无遮挡、部分遮挡、完全遮挡、墙体遮挡、重新露出和猎人在前的合成数据测试。游戏内轮廓、任务兼容性和帧时间仍待实测。

2026-11-25 在 Astris 中启动更新后的诊断包，日志确认游戏版本 1.4.0、subsdk3 装载及探针初始化。伤害显示、斩味条、3.HD 和任务视距 40% 同时启用。随后玩家进入单人任务。同一次启动在模拟时间 00:01:48 记录 slot 0 的猎人主体（part 9）更新和绘制，00:02:11 记录 part 1–8 全部更新和绘制。记录按部件和阶段去重，无法凭此区分具体武器、拔刀动作或三个网格调用点。该观察窗口内三个原版描边分支均未命中。


### 独立蒙版合成

`src/occluded_edge.frag` 接受三张同视口、同采样位置的正线性深度纹理，分别来自本地猎人、大型怪物和完整场景。前两张纹理中零表示未覆盖。合成时先从全部猎人部件的并集求外缘，再检查该像素最前方的场景深度是否与怪物深度吻合。游戏端使用两张独立 D24S8，并按原始深度量化精度比较。

在 macOS 以 OpenGL 4.1 离屏执行实际着色器的验证命令如下。

```sh
clang -std=c11 -Wall -Wextra -Werror tests/occluded_edge.c -framework OpenGL -o /tmp/mhgu-occlusion-gpu
/tmp/mhgu-occlusion-gpu
```

2026-11-25 本机通过六组逐像素比对，覆盖无遮挡、局部遮挡、完全遮挡、墙在怪物前、猎人在怪物前以及怪物移开。Astris 的 Switch 图形后端接入与实机遮挡效果仍待验证。

玩家随后在同一次任务中完成怪物完全遮挡并重新露出的对照。截至该会话日志 00:07:51.799，探针初始化一次，slot 0 的主体及八个部件共有 18 条首次更新或绘制记录，三个原版描边分支的命中记录合计为零。遮挡动作由玩家确认，日志未逐帧标记遮挡起止。

后续诊断包在网格绘制时精确匹配 1.4.0 类型表中 61 个编号形式为 uEmNNN_00 的怪物主体。每种首次匹配输出 `MHGU large monster NNN draw`。小怪 uEms 类、地形和未登记的派生类不计入。下一次启动在猎人与怪物同屏时记录到了 032 号主体。

### 游戏深度纹理实测

网格准备时读取绘制上下文的深度视图，提交端在 `main+0xBC1200` 调用 NVN SetRenderTargets 时读取实际深度纹理、视图、大小、格式及采样数。两端仅记录元数据，按纹理和视图标识关联。运行时地址只用于同一次游戏启动内的比对。

2026-11-25 新版在开场阶段捕获了 1920×1080 和 960×540 两张独立深度纹理，格式均为 DEPTH24_STENCIL8，采样数返回 0。可运行 `python3 tools/depth_report.py /path/to/Astris.log` 对照最近一次启动中猎人和大型怪物使用的目标与渲染阶段。列表中空的阶段表示该角色尚无对应观测。

合成着色器现支持直接解码投影深度。纹理必须来自同一摄像机、深度范围与视口，并显式提供清除值和深度解码系数。macOS 离屏 GPU 已通过线性、正向 Z 和反向 Z 共 18 组逐像素比对。此处记录的是旧诊断包。

### 独立深度绘制原型

在 Astris 的 21:38:16 会话中，任务内本地猎人与 032 号大型怪物共用默认视图的 1920×1080 D24S8 场景深度，均命中阶段 6、12 和 13。两者在另外一张 1024×3072 D32F 上也命中了 12 和 13。当前原型只从阶段 6、尺寸与渲染器视口一致的 D24S8 目标捕获网格。

`src/mask_capture.c` 使用游戏的纹理构造函数创建两张 D24S8。场景清深度时，将相同的深度清除值写入这两张纹理，再恢复原颜色与深度绑定。三处索引绘制调用在本地猎人或大型怪物命中时建立一条原生子命令列表，先用独立目标重绘，再执行原始命令。列表沿用原排序键及帧命令内存。帧命令内存不足时调用原版分配函数扩展，仅在分配失败时跳过重绘。

新包日志中，`MHGU private depth pair allocated` 表示游戏已创建两张独立纹理，`MHGU hunter depth replay queued` 和 `MHGU monster depth replay queued` 分别表示至少一个对应的网格已提交重绘。此前同屏会话使用上一版探针。新包中纹理的实际覆盖及游戏画面仍待验证。

### 游戏端合成与验证

`src/composite.c` 在主线程和渲染工作线程共用的原版绘制函数返回后执行。只有当前命令缓冲中确实清除了场景深度、绑定过两张独立目标、而且尺寸与交换链视口匹配，才会绘制。两次重绘采用原版材质和骨骼。着色器在猎人蒙版的内侧边缘判断同像素上大型怪物是否为最前方遮挡者，容差为两个 D24 量化单位。成功录制首次合成时输出 `MHGU occluded outline composite recorded`。该标记可验证命令流，最终呈现需要游戏截图确认。

构建直接使用 `src/shaders/` 内的自编 NVN 字节码。修改 GLSL 时，先使用 [UAM 的 NVN 分支](https://github.com/lincoln-lm/uam/tree/58c420b09a3edccd0df4b83456343679e08743d5) 重新编译。编译器会向工作目录写入符号 JSON，可在临时目录执行以下命令，`repo` 为本仓库绝对路径。

```sh
uam --glslcbinds -c "$repo/src/shaders/vertex.ctrl" -g "$repo/src/shaders/vertex.gpu" "$repo/src/fullscreen.vert"
uam --glslcbinds -c "$repo/src/shaders/edge.ctrl" -g "$repo/src/shaders/edge.gpu" "$repo/src/occluded_edge.frag"
```

在 macOS 构建上述 GPU 测试后，运行 `/tmp/mhgu-occlusion-gpu --native` 可额外验证原生路径使用的顶点着色器和 D24 深度比较。正向及反向深度共 12 例已通过。完整任务测试和帧时间对照仍待进行。

### Astris 任务内画面对照（2026-11-25）

本次运行使用 MHGU 1.4.0。日志中，00:02:21.244 创建两张独立深度纹理，00:02:21.248 提交本地猎人重绘，00:02:36.510 提交 032 号大型怪物重绘，00:02:45.576 首次记录轮廓合成命令。

在同一次任务的两帧中直接观察到：怪物躯干遮住画面中央的猎人时，其表面可见与猎人身体、武器及脚部吻合的青色外缘；恢复游戏后，猎人倒地并重新露出，猎人周围及原位置的地面上均未见青线。当前线条在高亮纹理上较细。本次实测覆盖遮挡和重新露出；部分遮挡、墙体反例、切区和帧时间仍缺少游戏内对照。

加粗版将原先 1 像素的青色轮廓向猎人轮廓内侧扩展至 3 个渲染像素。针对此宽度的 30 组离屏 GPU 逐像素对照已通过，游戏画面对照来自此前的细线版。

首次加粗版在游戏首次合成后，Astris 报告不支持 PRET 及主函数中的返回指令。修订版保留 3 像素圆盘采样，统一在着色器末尾输出。构建时检查片元机器码中的 PRET/RET；修订版通过 30 组离屏对照，仍待本次修订后的游戏画面验证。

低开销版本将三像素轮廓改为八方向采样，近期帧没有提交大型怪物时跳过猎人重绘及独立深度清除。默认关闭逐模型和逐纹理诊断。30 组离屏轮廓测试通过。旧会话第一次音频内存池告警出现在 00:00:08.207，第一次轮廓合成出现在 00:01:31.774。玩家在低开销版中复测，确认此前明显的声音滞后已消失。本次验证以实际听感为准。
