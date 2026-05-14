# API Reference<a name="EN-US_TOPIC_0000002522230512"></a>

## Data Structure Description<a name="EN-US_TOPIC_0000002522070512"></a>

- Enumerated variables of the multi-pattern rule matching engine:

    ```
    typedef enum {
        MMPM_MATCHER_NONE = 0,
        MMPM_MATCHER_TYPE_HASH,
        MMPM_MATCHER_TYPE_AC,
        MMPM_MATCHER_TYPE_MAX
    } mmpm_matcher_type;
    ```

- Error code of multi-pattern rule matching:

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

    **Table  1**  Error codes

|Error Code|Return Value|Description|Solution|
|--|--|--|--|
|MMPM_INVALID_PARA|-1|Null pointer or invalid rule, where **key_len** is 0 or is inconsistent with the actual key length.|Check the validity of input parameters for the reported function.|
|MMPM_MALLOC_FAIL|-2|Heap memory allocation failed.|Check if the system memory is heavily utilized by other programs, and if too many rules are input when building the matching engine.|
|MMPM_BUILT_ALREADY|-3|The matching engine has been successfully built. Do not build it again.|Check for erroneous multiple builds of the matching engine.|
|MMPM_FIND_FAIL|-4|Rule matching failed (the rule failed to be found in the matching engine).|-|
|MMPM_INIT_RWLOCK_FAIL|-5|Lock initialization failed when a hash-based matching engine is initialized.|Check if the system memory is heavily utilized by other programs.|
|MMPM_MEMCPY_FAIL|-6|Memory copy failed.|-|
|MMPM_NOT_SUPPORTED|-7|The handle of the AC matching engine is transferred to the rule removal function.|Check for any attempts to remove rules related to the AC matching engine.|


- Rule structure of multi-pattern rule matching:

    ```
    typedef struct {
        uint32_t alias; /* Rule alias, where duplicate names are allowed. */
        uint32_t key_len; 
        uint8_t *key; /* Rule matching key, which is the unique identifier. */
    } mmpm_rule_t;
    ```

- Matching engine handle data structure of multi-pattern rule matching:

    ```
    typedef struct {    
        void *mmpm_matcher; /* Matching engine handle */
        mmpm_matcher_type type; /* Matching engine type */
        bool is_built; /* Whether the matching engine is built. */
    } mmpm_handler_t;
    ```


## mmpm\_handler\_init<a name="EN-US_TOPIC_0000002522230514"></a>

**Feature<a name="section167323853"></a>**

Initializes the multi-pattern rule matching handler.

**Definition<a name="section18850182512619"></a>**

```
int mmpm_handler_init(mmpm_handler_t *handler, mmpm_matcher_type type, uint32_t buckets, uint32_t secondary_buckets);
```

**Parameter Description<a name="section1992151418176"></a>**

|**Parameter**|**Description**|**Value Range**|**Input/Output**|
|--|--|--|--|
|handler|Handler of the matching engine|Not null|Input|
|type|Type of the matching engine|Variables enumerated by mmpm_matcher_type|Input|
|buckets|Number of level-1 hash buckets. If the input value is **0**, the default configuration is used, that is, the number of level-1 hash buckets is 100,000.|Non-negative value. The value falls within the range of uint32_t.|Input|
|secondary_buckets|Number of level-2 hash buckets under each level-1 hash bucket. If the input value is **0**, the default configuration is used, that is, each level-1 hash bucket contains 96 level-2 hash buckets.|Non-negative value. The value falls within the range of uint32_t.|Input|


**Return Value<a name="section73338197172"></a>**

- Success:  **0**  is returned.
- Failure: The error code is returned.

**Constraints<a name="section10837102592211"></a>**

Do not call this function repeatedly on the same handler. If you want to reinitialize a handler, destroy the handler by calling mmpm\_handler\_destroy and then initialize the handler again by calling mmpm\_handler\_init.


## mmpm\_set\_hash\_func<a name="EN-US_TOPIC_0000002522070522"></a>

**Feature<a name="section1427919476610"></a>**

Registers a hash calculation function with the multi-patter rule matching engine.

**Definition<a name="section077114561764"></a>**

```
int mmpm_set_hash_func(mmpm_handler_t *handler, hash_calc_func hash_func);
```

**Parameter Description<a name="section1992151418176"></a>**

|**Parameter**|**Description**|**Value Range**|**Input/Output**|
|--|--|--|--|
|handler|Handler of the matching engine|Not null|Input|
|hash_func|Hash function pointer|Not null|Input|


**Return Value<a name="section73338197172"></a>**

- Success:  **0**  is returned.
- Failure: The error code is returned.


## mmpm\_handler\_destroy<a name="EN-US_TOPIC_0000002522230520"></a>

**Feature<a name="section17306518720"></a>**

Releases resources transferred to the multi-pattern rule matching engine.

**Definition<a name="section984215183719"></a>**

```
int mmpm_handler_destroy(mmpm_handler_t *handler);
```

**Parameter Description<a name="section1992151418176"></a>**

|**Parameter**|**Description**|**Value Range**|**Input/Output**|
|--|--|--|--|
|handler|Handler of the matching engine|Not null|Input|


**Return Value<a name="section73338197172"></a>**

- Success:  **0**  is returned.
- Failure: The error code is returned.


## mmpm\_add\_rule<a name="EN-US_TOPIC_0000002553350433"></a>

**Feature<a name="section77471153978"></a>**

Imports a rule set to the multi-pattern rule matching engine.  **rule-\>key**  and  **rule-\>key\_len**  must have a one-to-one correspondence to ensure the function can work as expected.

**Definition<a name="section11961501883"></a>**

```
int mmpm_add_rule(mmpm_handler_t *handler, mmpm_rule_t *rule, uint32_t rule_num);
```

**Parameter Description<a name="section1992151418176"></a>**

|**Parameter**|**Description**|**Value Range**|**Input/Output**|
|--|--|--|--|
|handler|Handler of the matching engine|Not null|Input|
|rule|Array of rules to be imported|Not null|Input|
|rule_num|Number of imported rules|Non-negative value. The value falls within the range of uint32_t, and matches the actual length of the rule array, otherwise, an out-of-bounds access exception will occur.|Input|


**Return Value<a name="section73338197172"></a>**

- Success:  **0**  is returned.
- Failure: The error code is returned.


## mmpm\_remove\_rule<a name="EN-US_TOPIC_0000002553190489"></a>

**Feature<a name="section6912101017818"></a>**

Removes a target rule from the multi-pattern rule matching engine.  **rule-\>key**  and  **rule-\>key\_len**  must have a one-to-one correspondence to ensure the function can work as expected.

**Definition<a name="section147941817387"></a>**

```
int mmpm_remove_rule(mmpm_handler_t *handler, mmpm_rule_t *rule, uint32_t rule_num);
```

**Parameter Description<a name="section1992151418176"></a>**

|**Parameter**|**Description**|**Value Range**|**Input/Output**|
|--|--|--|--|
|handler|Handler of the matching engine|Not null|Input|
|rule|Array of rules to be removed|Not null|Input|
|rule_num|Number of rules to be removed|Non-negative value. The value falls within the range of uint32_t, and matches the actual length of the rule array, otherwise, an out-of-bounds access exception will occur.|Input|


**Return Value<a name="section73338197172"></a>**

- Success:  **0**  is returned.
- Failure: The error code is returned.

**Constraints<a name="section16630311902"></a>**

Only the matching engine MMPM\_MATCHER\_TYPE\_HASH is supported.


## mmpm\_find\_rule<a name="EN-US_TOPIC_0000002522230524"></a>

**Feature<a name="section11527311084"></a>**

Finds the target rule using the multi-pattern rule matching engine.  **rule-\>key**  and  **rule-\>key\_len**  must have a one-to-one correspondence to ensure the function can work as expected.

**Definition<a name="section123011441986"></a>**

```
int mmpm_find_rule(mmpm_handler_t *handler, mmpm_rule_t *rule);
```

**Parameter Description<a name="section1992151418176"></a>**

|**Parameter**|**Description**|**Value Range**|**Input/Output**|
|--|--|--|--|
|handler|Handler of the matching engine|Not null|Input|
|rule|Rules to be matched|Not null|Input/Output|


**Return Value<a name="section73338197172"></a>**

- Success:  **0**  is returned, and the alias of the matched rule is returned through  **rule-\>alias**.
- Failure: The error code is returned.


