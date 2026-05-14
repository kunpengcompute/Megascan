# API参考<a name="ZH-CN_TOPIC_0000002522230512"></a>

## 数据结构说明<a name="ZH-CN_TOPIC_0000002522070512"></a>

- 多模规则匹配引擎枚举变量：

    ```
    typedef enum {
        MMPM_MATCHER_NONE = 0,
        MMPM_MATCHER_TYPE_HASH,
        MMPM_MATCHER_TYPE_AC,
        MMPM_MATCHER_TYPE_MAX
    } mmpm_matcher_type;
    ```

- 多模规则匹配错误码：

    ```
    #define MMPM_SUCCESS            0
    #define MMPM_INVALID_PARA       (-1)
    #define MMPM_MALLOC_FAIL        (-2)
    #define MMPM_BUILT_ALREADY      (-3)
    #define MMPM_FIND_FAIL          (-4)
    #define MMPM_INIT_RWLOCK_FAIL   (-5)
    #define MMPM_MEMCPY_FAIL        (-6)
    #define MMPM_NOT_SUPPORTED      (-7)
    ```

    **表 1** 错误码说明<a id="错误码说明"></a>

|错误码|返回值|解释|解决办法|
|--|--|--|--|
|MMPM_INVALID_PARA|-1|传入空指针，或者传入不合法的规则。例：传入的key_len为0，或者与key的实际长度不一致|根据报错的函数，检查对应的入参是否合法|
|MMPM_MALLOC_FAIL|-2|堆内存申请失败|检查系统内存是否被其他程序大量占用；检查构建匹配引擎时是否传入过多规则|
|MMPM_BUILT_ALREADY|-3|匹配引擎已构建成功，禁止重复构建|检查是否存在多次构建匹配引擎的错误使用方式|
|MMPM_FIND_FAIL|-4|规则匹配失败，匹配引擎中不存在该规则|-|
|MMPM_INIT_RWLOCK_FAIL|-5|在初始化Hash匹配引擎时，锁初始化失败|检查系统内存是否被其他程序大量占用|
|MMPM_MEMCPY_FAIL|-6|内存复制失败|-|
|MMPM_NOT_SUPPORTED|-7|在移除规则函数中传入AC匹配引擎的句柄|检查是否有尝试移除AC匹配引擎相关规则的行为|


- 多模规则匹配，规则数据结构：

    ```
    typedef struct {
        uint32_t alias; /* 规则别名，可重复 */
        uint32_t key_len; 
        uint8_t *key; /* 规则匹配key，唯一标识 */
    } mmpm_rule_t;
    ```

- 多模规则匹配，匹配引擎句柄数据结构：

    ```
    typedef struct {    
        void *mmpm_matcher; /* 匹配引擎句柄 */
        mmpm_matcher_type type; /* 匹配引擎种类 */
        bool is_built; /* 匹配引擎是否已构建 */
    } mmpm_handler_t;
    ```


## mmpm\_handler\_init<a name="ZH-CN_TOPIC_0000002522230514"></a>

**接口功能<a name="section167323853"></a>**

初始化多模规则匹配handler。

**接口定义<a name="section18850182512619"></a>**

```
int mmpm_handler_init(mmpm_handler_t *handler, mmpm_matcher_type type, uint32_t buckets, uint32_t secondary_buckets);
```

**参数说明<a name="section1992151418176"></a>**

|**参数名**|**描述**|**取值范围**|**输入/输出**|
|--|--|--|--|
|handler|匹配引擎句柄|非NULL|输入|
|type|匹配引擎种类|mmpm_matcher_type枚举变量取值范围|输入|
|buckets|一级hash桶数量；输入为0则使用默认配置：10万一级桶|非负值，取值范围与uint32_t保持一致|输入|
|secondary_buckets|每个一级桶下的二级hash桶数量；输入为0则使用默认配置：每个一级桶下96个二级桶|非负值，取值范围与uint32_t保持一致|输入|


**返回值<a name="section73338197172"></a>**

- 成功：返回0。
- 失败：返回错误码。

**约束<a name="section10837102592211"></a>**

请勿对同一句柄连续调用此函数。如要对同一句柄重新初始化，请先执行句柄销毁mmpm\_handler\_destroy，然后再重新执行mmpm\_handler\_init初始化。


## mmpm\_set\_hash\_func<a name="ZH-CN_TOPIC_0000002522070522"></a>

**接口功能<a name="section1427919476610"></a>**

多模规则匹配引擎注册哈希计算函数。

**接口定义<a name="section077114561764"></a>**

```
int mmpm_set_hash_func(mmpm_handler_t *handler, hash_calc_func hash_func);
```

**参数说明<a name="section1992151418176"></a>**

|**参数名**|**描述**|**取值范围**|**输入/输出**|
|--|--|--|--|
|handler|匹配引擎句柄|非NULL|输入|
|hash_func|哈希函数指针|非NULL|输入|


**返回值<a name="section73338197172"></a>**

- 成功：返回0。
- 失败：返回错误码。


## mmpm\_handler\_destroy<a name="ZH-CN_TOPIC_0000002522230520"></a>

**接口功能<a name="section17306518720"></a>**

多模规则匹配引擎资源释放函数，释放传入的多模规则匹配引擎的资源。

**接口定义<a name="section984215183719"></a>**

```
int mmpm_handler_destroy(mmpm_handler_t *handler);
```

**参数说明<a name="section1992151418176"></a>**

|**参数名**|**描述**|**取值范围**|**输入/输出**|
|--|--|--|--|
|handler|匹配引擎句柄|非NULL|输入|


**返回值<a name="section73338197172"></a>**

- 成功：返回0。
- 失败：返回错误码。


## mmpm\_add\_rule<a name="ZH-CN_TOPIC_0000002553350433"></a>

**接口功能<a name="section77471153978"></a>**

多模规则匹配引擎导入规则集合。rule-\>key和rule-\>key\_len需要一一对应，否则无法确保功能符合预期。

**接口定义<a name="section11961501883"></a>**

```
int mmpm_add_rule(mmpm_handler_t *handler, mmpm_rule_t *rule, uint32_t rule_num);
```

**参数说明<a name="section1992151418176"></a>**

|**参数名**|**描述**|**取值范围**|**输入/输出**|
|--|--|--|--|
|handler|匹配引擎句柄|非NULL|输入|
|rule|待导入的规则数组|非NULL|输入|
|rule_num|导入规则数量|非负值，取值范围与uint32_t保持一致；值必须与参数rule的实际数组长度一致，否则会出现访问越界异常|输入|


**返回值<a name="section73338197172"></a>**

- 成功：返回0。
- 失败：返回错误码。


## mmpm\_remove\_rule<a name="ZH-CN_TOPIC_0000002553190489"></a>

**接口功能<a name="section6912101017818"></a>**

多模规则匹配引擎删除目标规则。rule-\>key和rule-\>key\_len需要一一对应，否则无法确保功能符合预期。

**接口定义<a name="section147941817387"></a>**

```
int mmpm_remove_rule(mmpm_handler_t *handler, mmpm_rule_t *rule, uint32_t rule_num);
```

**参数说明<a name="section1992151418176"></a>**

|**参数名**|**描述**|**取值范围**|**输入/输出**|
|--|--|--|--|
|handler|匹配引擎句柄|非NULL|输入|
|rule|待删除的规则数组|非NULL|输入|
|rule_num|删除规则数量|非负值，取值范围与uint32_t保持一致值必须与参数rule的实际数组长度一致；否则会出现访问越界异常|输入|


**返回值<a name="section73338197172"></a>**

- 成功：返回0。
- 失败：返回错误码。

**约束<a name="section16630311902"></a>**

仅支持匹配引擎为“MMPM\_MATCHER\_TYPE\_HASH”。


## mmpm\_find\_rule<a name="ZH-CN_TOPIC_0000002522230524"></a>

**接口功能<a name="section11527311084"></a>**

多模规则匹配引擎匹配目标规则。rule-\>key和rule-\>key\_len需要一一对应，否则无法确保功能符合预期。

**接口定义<a name="section123011441986"></a>**

```
int mmpm_find_rule(mmpm_handler_t *handler, mmpm_rule_t *rule);
```

**参数说明<a name="section1992151418176"></a>**

|**参数名**|**描述**|**取值范围**|**输入/输出**|
|--|--|--|--|
|handler|匹配引擎句柄|非NULL|输入|
|rule|待匹配规则|非NULL|输入/输出|


**返回值<a name="section73338197172"></a>**

- 成功：返回0，且命中规则别名通过rule-\>alias返回。
- 失败：返回错误码。


