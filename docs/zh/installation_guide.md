# 安装指南<a name="ZH-CN_TOPIC_0000002553346771"></a>

## 环境要求<a name="ZH-CN_TOPIC_0000002522226848"></a>

为保证您可以顺利安全地使用KDADK-MMPM，请确保所使用的环境信息在已验证环境范围内。若您在使用过程中遇到问题，建议先检查使用的环境能否切换为已验证的环境范围。

**表 1** 已验证环境<a id="已验证环境"></a>

|操作系统|CPU类型|GCC版本|
|--|--|--|
|openEuler 22.03 LTS SP4|鲲鹏920新型号处理器|10.3.1|
|openEuler 24.03 LTS SP1|鲲鹏920新型号处理器|12.3.1|


**表 2** 软件要求<a id="软件要求"></a>

|软件名称|版本|获取方式|
|--|--|--|
|Boostkit-boostida-kdadk-mmpm-2.5.3.zip|2.5.3|请前往“智能数据分析>资源下载”页面获取软件包。|


为保证您可以顺利安全地使用KDADK-MMPM，请确保所使用的环境信息在已验证环境范围内。若您在使用过程中遇到问题，建议先检查使用的环境能否切换为已验证的环境范围。
## 软件部署<a name="ZH-CN_TOPIC_0000002553186817"></a>

**软件安装包获取与完整性校验<a name="section5964123113114"></a>**

1. 从鲲鹏社区获取对应的软件数字证书和软件安装包。

    参见[**表 2** 软件要求](#软件要求)获取**Boostkit-boostida-kdadk-mmpm-2.5.3.zip**软件包。

    >![](public_sys-resources/icon-note.gif) **说明：** 
    >使用软件包前请先阅读《[鲲鹏应用使能套件BoostKit用户许可协议 2.0](https://www.hikunpeng.com/zh/legal/developer/boostkit/software/protocol)》，如确认继续使用，则默认同意协议的条款和条件。

2. <a name="li11603165572014"></a>获取[校验工具和校验方法](https://support.huawei.com/enterprise/zh/tool/pgp-verify-TL1000000054)。
3. 请参考[2](#li11603165572014)中下载的《OpenPGP签名验证指南》进行软件包完整性检查。

**安装软件包<a name="section3965331101111"></a>**

KDADK-MMPM（Million-rule Multi-Pattern Matcher）是多模规则匹配软件包，可以提升超大规模定长规则匹配或十万不定长规则匹配性能。

1. 请参见[**表 2** 软件要求](#软件要求)获取KDADK-MMPM相关软件包**Boostkit-boostida-kdadk-mmpm-2.5.3.zip**。
2. 解压缩**Boostkit-boostida-kdadk-mmpm-2.5.3.zip**，得到KDADK-MMPM二进制RPM包。
3. 安装KDADK-MMPM。

    ```
    rpm -ivh mmpm-xxxx.aarch64.rpm
    ```

    ![](figures/1.png)

    操作结束后，会将动态库安装至“/usr/local/lib“，对外函数接口头文件安装至“/usr/local/include“。

    >![](public_sys-resources/icon-note.gif) **说明：** 
    >上述命令中涉及到的xxxx代表版本号。

4. 使能KDADK-MMPM。

    使用KDADK-MMPM时需在程序中包含mmpm.h头文件并调用KDADK-MMPM相关函数。以GCC为例，编译链接时添加-lmmpm参数即可完成库的链接。详细使用方法可参见[zh-cn\_topic\_0000002553346765.md](zh-cn_topic_0000002553346765.md)中的示例。

    ```
    gcc your_program.c -o your_program -lmmpm
    ```

    若编译成功且程序能够正常运行，表明已成功使能KDADK-MMPM。


