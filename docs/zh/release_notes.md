# 版本说明书<a name="ZH-CN_TOPIC_0000002522095666"></a>

## 修改记录<a name="ZH-CN_TOPIC_0000002522095667"></a>

<a name="table_revision_history"></a>
<table><tbody><tr id="row_revision_header"><th class="firstcol" valign="top" width="26%" id="revision_date"><p>发布日期</p>
</th>
<th class="cellrowborder" valign="top" width="74%" id="revision_description"><p>修改记录</p>
</th>
</tr>
<tr id="row_revision_100"><td class="firstcol" valign="top" width="26%" headers="revision_date"><p>2026-09-30</p>
</td>
<td class="cellrowborder" valign="top" width="74%" headers="revision_description"><p>第一次正式发布。</p>
</td>
</tr>
</tbody>
</table>

## 版本配套说明<a name="ZH-CN_TOPIC_0000002521728672"></a>

### 产品版本信息<a name="ZH-CN_TOPIC_0000002552768617"></a>

<a name="table216mcpsimp"></a>
<table><tbody><tr id="row221mcpsimp"><th class="firstcol" valign="top" width="26%" id="mcps1.1.3.1.1"><p id="p223mcpsimp"><a name="p223mcpsimp"></a><a name="p223mcpsimp"></a>产品名称</p>
</th>
<td class="cellrowborder" valign="top" width="74%" headers="mcps1.1.3.1.1 "><p id="p225mcpsimp"><a name="p225mcpsimp"></a><a name="p225mcpsimp"></a>Kunpeng BoostKit</p>
</td>
</tr>
<tr id="row226mcpsimp"><th class="firstcol" valign="top" width="26%" id="mcps1.1.3.2.1"><p id="p228mcpsimp"><a name="p228mcpsimp"></a><a name="p228mcpsimp"></a>产品版本</p>
</th>
<td class="cellrowborder" valign="top" width="74%" headers="mcps1.1.3.2.1 "><p id="p230mcpsimp"><a name="p230mcpsimp"></a><a name="p230mcpsimp"></a>26.2.RC1</p>
</td>
</tr>
<tr id="row231mcpsimp"><th class="firstcol" valign="top" width="26%" id="mcps1.1.3.3.1"><p id="p233mcpsimp"><a name="p233mcpsimp"></a><a name="p233mcpsimp"></a>软件名称</p>
</th>
<td class="cellrowborder" valign="top" width="74%" headers="mcps1.1.3.3.1 "><p id="p1773655186"><a name="p1773655186"></a><a name="p1773655186"></a>Megascan</p>
</td>
</tr>
<tr id="row236mcpsimp"><th class="firstcol" valign="top" width="26%" id="mcps1.1.3.4.1"><p id="p238mcpsimp"><a name="p238mcpsimp"></a><a name="p238mcpsimp"></a>软件版本</p>
</th>
<td class="cellrowborder" valign="top" width="74%" headers="mcps1.1.3.4.1 "><p id="p240mcpsimp"><a name="p240mcpsimp"></a><a name="p240mcpsimp"></a>1.0.0</p>
</td>
</tr>
</tbody>
</table>

### 与操作系统和数据库配套说明<a name="ZH-CN_TOPIC_0000002552768619"></a>

- 支持openEuler 22.03 LTS SP4、openEuler 24.03 LTS SP1版本。
- 不涉及数据库配套。

## V1.0.0<a name="ZH-CN_TOPIC_0000002552888637"></a>

### 更新说明<a name="ZH-CN_TOPIC_0000002521888672"></a>

**新增特性<a name="section1863910161560"></a>**

<a name="table_new_features_100"></a>
<table><tbody><tr id="row_new_features_header"><th class="firstcol" valign="top" width="30%" id="new_feature_name"><p><strong>特性名称</strong></p>
</th>
<th class="cellrowborder" valign="top" width="70%" id="new_feature_description"><p><strong>特性说明</strong></p>
</th>
</tr>
<tr id="row_new_feature_large_scale"><td class="firstcol" valign="top" width="30%" headers="new_feature_name"><p>大规模规则集匹配技术</p>
</td>
<td class="cellrowborder" valign="top" width="70%" headers="new_feature_description"><p>针对规则规模和规则长度的差异，结合Hash算法与紧凑AC自动机，降低大规模规则集匹配过程中的查找和状态访问开销。</p>
</td>
</tr>
<tr id="row_new_feature_combination"><td class="firstcol" valign="top" width="30%" headers="new_feature_name"><p>组合规则增量求值技术</p>
</td>
<td class="cellrowborder" valign="top" width="70%" headers="new_feature_description"><p>通过普通规则到组合规则的反向索引，仅计算本次命中所影响的组合关系，减少大规模组合规则场景中的无效遍历。</p>
</td>
</tr>
<tr id="row_new_feature_stream"><td class="firstcol" valign="top" width="30%" headers="new_feature_name"><p>跨分片状态复用技术</p>
</td>
<td class="cellrowborder" valign="top" width="70%" headers="new_feature_description"><p>通过stream保存自动机和组合规则状态，并复用scratch工作区，使连续数据能够跨任意输入分片完成匹配。</p>
</td>
</tr>
</tbody>
</table>

## 版本配套文档<a name="ZH-CN_TOPIC_0000002521728676"></a>

### 版本配套文档<a name="ZH-CN_TOPIC_0000002521888670"></a>

<a name="table_supporting_documents"></a>
<table><tbody><tr id="row_supporting_documents_header"><th class="firstcol" valign="top" width="30%" id="document_name"><p><strong>文档名称</strong></p>
</th>
<th class="cellrowborder" valign="top" width="50%" id="document_description"><p><strong>内容简介</strong></p>
</th>
<th class="cellrowborder" valign="top" width="20%" id="document_delivery"><p><strong>交付形式</strong></p>
</th>
</tr>
<tr id="row_document_release_notes"><td class="firstcol" valign="top" width="30%" headers="document_name"><p>版本说明书</p>
</td>
<td class="cellrowborder" valign="top" width="50%" headers="document_description"><p>提供Megascan版本发布信息。</p>
</td>
<td class="cellrowborder" valign="top" width="20%" headers="document_delivery"><p>开源仓</p>
</td>
</tr>
<tr id="row_document_installation"><td class="firstcol" valign="top" width="30%" headers="document_name"><p>安装指南</p>
</td>
<td class="cellrowborder" valign="top" width="50%" headers="document_description"><p>提供Megascan编译、安装和测试指导。</p>
</td>
<td class="cellrowborder" valign="top" width="20%" headers="document_delivery"><p>开源仓</p>
</td>
</tr>
<tr id="row_document_quick_start"><td class="firstcol" valign="top" width="30%" headers="document_name"><p>快速入门</p>
</td>
<td class="cellrowborder" valign="top" width="50%" headers="document_description"><p>提供Megascan基础接口调用示例。</p>
</td>
<td class="cellrowborder" valign="top" width="20%" headers="document_delivery"><p>开源仓</p>
</td>
</tr>
<tr id="row_document_api"><td class="firstcol" valign="top" width="30%" headers="document_name"><p>API参考</p>
</td>
<td class="cellrowborder" valign="top" width="50%" headers="document_description"><p>提供Megascan接口定义、参数和使用约束。</p>
</td>
<td class="cellrowborder" valign="top" width="20%" headers="document_delivery"><p>开源仓</p>
</td>
</tr>
</tbody>
</table>

### 获取文档的方法<a name="ZH-CN_TOPIC_0000002552888641"></a>

您可以通过访问[开源仓](https://gitcode.com/boostkit/Megascan)浏览和获取相关文档。
