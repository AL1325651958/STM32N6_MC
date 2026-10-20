# MiniCraft 物品图标

本目录的图标为项目原创像素素材，由 `tools/make_minicraft_icons.py` 绘制，不使用外部游戏贴图。

- `items.png`：按 `minicraft.h` 的物品 ID 排列，单图 16×16，RGBA 透明底。
- `items-sheet.png`：用于查看整套图标的放大预览，带名称与 ID。
- `Appli/Core/Inc/mc_icons.h`：生成的运行时资源。每物品 128 字节 4 位像素索引及 32 字节 RGB565 调色板，索引 0 透明，共 4960 字节。固件构建直接使用该文件，不依赖 Pillow 或 SD 卡。

如需修改造型，编辑生成脚本后，在工程根目录执行：

```powershell
F:\Python310\python.exe tools/make_minicraft_icons.py
```

生成脚本需要 Pillow。新增物品时需同步脚本中的 ID、头文件与存档格式约束；不要重排现有 ID。
