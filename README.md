# Megascan介绍<a name="ZH-CN_TOPIC_0000002553148843"></a>

## 项目介绍<a name="ZH-CN_TOPIC_0000002553028849"></a>

Megascan是面向大规模规则集的多模式匹配库，结合Hash算法与AC算法，适用于超大规模定长规则和十万级不定长规则匹配场景。Megascan提供兼容handler接口，以及database、scratch和stream组成的扫描接口，支持普通规则、组合规则、Block扫描、跨分片Stream扫描和动态删除。


## 目录结构<a name="ZH-CN_TOPIC_0000002522188896"></a>

```
├── ms                                            # Megascan源文件目录
│   ├── include/                                  # Megascan公开头文件目录
│   ├── src                                       # Megascan源码实现目录
│   ├── CMakeLists.txt                            # 构建配置
├── docs                                          # 项目文档目录
│   ├── en                                        # 英文文档目录
│   └── zh                                        # 中文文档目录
│       ├── figures                               # 中文文档图片资源目录
│       ├── quick_start.md                        # 快速入门
│       ├── release_notes.md                      # 版本说明书
│       ├── installation_guide.md                 # 安装指南
│       ├── api_reference.md                      # API参考
├── spec                                          # Megascan库RPM打包工具
├── test                                          # 测试目录
│   ├── demo/                                     # API示例和性能测试目录
│   ├── mock/                                     # mock目录
│   ├── public/                                   # 公共头文件
│   ├── script/                                   # 脚本目录
│   ├── src/                                      # ut源文件目录
│   ├── CMakeLists.txt                            # 测试目录构建配置
│   ├── README.md                                 # KTest测试框架说明文档
│   ├── ...                                       # 其他文件
├── CMakeLists.txt                                # 项目构建配置
├── README.md                                     # 项目说明文档
└── ...                                           # 其他根级文件
```


## 版本说明<a name="ZH-CN_TOPIC_0000002553148847"></a>

每个版本的特性变更详细信息，具体请参见[《版本说明书》](./docs/zh/release_notes.md)。


## 环境部署<a name="ZH-CN_TOPIC_0000002522028894"></a>

编译Megascan动态库需要GCC 10.3及以上版本、CMake 3.22及以上版本和GNU Make，不依赖Boost、PCRE等第三方算法库。在openEuler上可安装基础编译工具：

```bash
sudo dnf install -y gcc gcc-c++ cmake make
```

在源码根目录执行：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

编译完成后，动态库位于`output/lib/libms.so`，对外头文件位于`output/include`。RPM安装、测试依赖和详细编译步骤参见[安装指南](./docs/zh/installation_guide.md)。


## 快速入门<a name="ZH-CN_TOPIC_0000002522028892"></a>

Megascan的快速入门通过使用提供的接口来具体上手应用，具体请参见[《快速入门》](./docs/zh/quick_start.md)。


## 学习文档<a name="ZH-CN_TOPIC_0000002553028853"></a>

|名称|路径|简介|
|--|--|--|
|快速入门|[quick_start.md](./docs/zh/quick_start.md)|提供快速上手验证指导。|
|版本说明书|[release_notes.md](./docs/zh/release_notes.md)|提供Megascan每个发布版本的基础信息和特性更新信息。|
|安装指南|[installation_guide.md](./docs/zh/installation_guide.md)|指导用户如何安装部署软件。|
|API参考|[api_reference.md](./docs/zh/api_reference.md)|提供Megascan特性相关接口说明及定义等。|
|测试指南|[test/README.md](./test/README.md)|提供KTest、单元测试和demo的构建运行方法。|
|Demo使用说明|[test/demo/README.md](./test/demo/README.md)|提供API、session和性能测试入口说明。|



## 安全声明<a name="ZH-CN_TOPIC_0000002522228298"></a>

通过安全检查与加固提高系统或网络的安全性和稳定性，防止黑客攻击、数据泄露、系统崩溃等问题的发生。同时，也可以满足法规和合规要求，保护用户隐私和信息安全。

**缓冲区溢出安全保护<a name="section14653204420151"></a>**

此项仅Megascan特性需关注。

为阻止缓冲区溢出攻击，建议使用ASLR（Address space layout randomization）技术，通过对堆、栈、共享库映射等线性区布局的随机化，增加攻击者预测目的地址的难度，防止攻击者直接定位攻击代码位置。该技术可作用于堆、栈、内存映射区（mmap基址、shared libraries、vdso页）。

开启方式：

```
echo 2 >/proc/sys/kernel/randomize_va_space
```


## 贡献声明<a name="ZH-CN_TOPIC_0000002553148851"></a>

如果使用过程中有任何问题，或者需要反馈特性需求和bug报告，可以提交Issue联系我们，具体贡献方法可参考[这里](https://gitcode.com/boostkit/community/blob/master/docs/contributor/contributing.md)。

1. 提交错误报告：如果您在Megascan中发现了一个不存在安全问题的漏洞，请在Megascan仓库中的Issues中搜索，以防该漏洞被重复提交，如果找不到漏洞可以创建一个新的Issues。如果发现了一个安全问题请不要将其公开，请参见《安全问题处理方式》。提交错误报告时应该包含完整信息。
2. 安全问题处理：本项目中对安全问题处理的形式，请通过邮箱通知项目核心人员确认编辑。
3. 解决现有问题：通过查看仓库的Issues列表可以发现需要处理的问题信息，可以尝试解决其中的某个问题。
4. 如何提出新功能：请使用Issues的Feature标签进行标记，我们会定期处理和确认开发。
5. 开始贡献：
    1. Fork本项目的仓库。
    2. Clone到本地。
    3. 创建开发分支。
    4. 本地测试：提交前请通过所有单元测试，包括新增的测试用例。
    5. 提交代码。
    6. 新建Pull Request。
    7. 代码检视：您需要根据评审意见修改代码，并重新提交更新。此流程可能涉及多轮迭代。
    8. 当您的PR获得足够数量的检视者批准后，Committer会进行最终审核。
    9. 审核和测试通过后，CI会将您的PR合并入到项目的主干分支。


## 免责声明<a name="ZH-CN_TOPIC_0000002522188894"></a>

**致Megascan使用者**

- 本软件仅供调试和开发之用，使用者需自行承担使用风险，并理解以下内容：
    - 数据处理及删除：用户在使用本软件过程中产生的数据属于用户责任范畴。建议用户在使用完毕后及时删除相关数据，以防信息泄露。
    - 数据保密与传播：使用者了解并同意不得将通过本软件产生的数据随意外发或传播。对于由此产生的信息泄露、数据泄露或其他不良后果，本软件及其开发者概不负责。
    - 用户输入安全性：用户需自行保证输入的命令行的安全性，并承担因输入不当而导致的任何安全风险或损失。对于输入命令行不当所导致的问题，本软件及其开发者概不负责。

- 免责声明范围：本免责声明适用于所有使用本软件的个人或实体。使用本软件即表示您同意并接受本声明的内容，并愿意承担因使用该功能而产生的风险和责任，如有异议请停止使用本软件。
- 在使用本软件之前，请**谨慎阅读并理解以上免责声明的内容**。对于使用本软件所产生的任何问题或疑问，请及时联系开发者。

**致数据所有者**

如果您不希望您的数据集等信息在Megascan中被提及，或希望更新Megascan中有关的描述，请在GitCode提交Issue，我们将根据您的Issue要求删除或更新您相关描述。衷心感谢您对Megascan的理解和贡献。


## License<a name="ZH-CN_TOPIC_0000002522028888"></a>

本项目采用Private Repository Source Code License 1.0，具体请参见[LICENSE](LICENSE)文件。
