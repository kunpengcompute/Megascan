# Release Notes<a name="EN-US_TOPIC_0000002522095666"></a>

<!-- md-trans-meta sourceCommit=81bf84c25e421df10caeebc70b47da75562302f7 translatedAt=2026-08-31T01:42:02.156Z pushedAt=2026-09-04T09:29:33.188Z -->

## Change History<a name="EN-US_TOPIC_0000002522095667"></a>

<a name="table_revision_history"></a>

<table><tbody><tr id="row_revision_header"><th class="firstcol" valign="top" width="26%" id="revision_date"><p>Date</p></th>
<th class="cellrowborder" valign="top" width="74%" id="revision_description"><p>Description</p></th>
</tr>
<tr id="row_revision_100"><td class="firstcol" valign="top" width="26%" headers="revision_date"><p>2026-09-30</p></td>
<td class="cellrowborder" valign="top" width="74%" headers="revision_description"><p>This is the first official release.</p></td>
</tr>
</tbody>
</table>

## Version Mapping<a name="EN-US_TOPIC_0000002521728672"></a>

### Product Version Information<a name="EN-US_TOPIC_0000002552768617"></a>

<a name="table216mcpsimp"></a>

<table><tbody><tr id="row221mcpsimp"><th class="firstcol" valign="top" width="26%" id="mcps1.1.3.1.1"><p id="p223mcpsimp"><a name="p223mcpsimp"></a><a name="p223mcpsimp"></a>Product Name</p></th>
<td class="cellrowborder" valign="top" width="74%" headers="mcps1.1.3.1.1 "><p id="p225mcpsimp"><a name="p225mcpsimp"></a><a name="p225mcpsimp"></a>Kunpeng BoostKit</p></td>
</tr>
<tr id="row226mcpsimp"><th class="firstcol" valign="top" width="26%" id="mcps1.1.3.2.1"><p id="p228mcpsimp"><a name="p228mcpsimp"></a><a name="p228mcpsimp"></a>Product Version</p></th>
<td class="cellrowborder" valign="top" width="74%" headers="mcps1.1.3.2.1 "><p id="p230mcpsimp"><a name="p230mcpsimp"></a><a name="p230mcpsimp"></a>26.2.RC1</p></td>
</tr>
<tr id="row231mcpsimp"><th class="firstcol" valign="top" width="26%" id="mcps1.1.3.3.1"><p id="p233mcpsimp"><a name="p233mcpsimp"></a><a name="p233mcpsimp"></a>Software Name</p></th>
<td class="cellrowborder" valign="top" width="74%" headers="mcps1.1.3.3.1 "><p id="p1773655186"><a name="p1773655186"></a><a name="p1773655186"></a>Megascan</p></td>
</tr>
<tr id="row236mcpsimp"><th class="firstcol" valign="top" width="26%" id="mcps1.1.3.4.1"><p id="p238mcpsimp"><a name="p238mcpsimp"></a><a name="p238mcpsimp"></a>Software Version</p></th>
<td class="cellrowborder" valign="top" width="74%" headers="mcps1.1.3.4.1 "><p id="p240mcpsimp"><a name="p240mcpsimp"></a><a name="p240mcpsimp"></a>1.0.0</p></td>
</tr>
</tbody>
</table>

### OS and Database<a name="EN-US_TOPIC_0000002552768619"></a>

- openEuler 22.03 LTS SP4 and openEuler 24.03 LTS SP1 are supported.

- No database is involved.

## V1.0.0<a name="EN-US_TOPIC_0000002552888637"></a>

### Change Description<a name="EN-US_TOPIC_0000002521888672"></a>

**New Features<a name="section1863910161560"></a>**

<a name="table_new_features_100"></a>

<table><tbody><tr id="row_new_features_header"><th class="firstcol" valign="top" width="30%" id="new_feature_name"><p><strong>Feature Name</strong></p></th>
<th class="cellrowborder" valign="top" width="70%" id="new_feature_description"><p><strong>Feature Description</strong></p></th>
</tr>
<tr id="row_new_feature_large_scale"><td class="firstcol" valign="top" width="30%" headers="new_feature_name"><p>Large-scale rule set matching</p></td>
<td class="cellrowborder" valign="top" width="70%" headers="new_feature_description"><p>To address differences in rule set scale and rule length, this technology combines a hashing algorithm and a compact AC automaton to reduce lookup and state access overhead during large-scale rule set matching.</p></td>
</tr>
<tr id="row_new_feature_combination"><td class="firstcol" valign="top" width="30%" headers="new_feature_name"><p>Incremental evaluation for composite rules</p></td>
<td class="cellrowborder" valign="top" width="70%" headers="new_feature_description"><p>Through a reverse index from basic rules to composite rules, this technology only evaluates the composite relationships affected by the current match, reducing unnecessary traversal in large-scale composite rule scenarios.</p></td>
</tr>
<tr id="row_new_feature_stream"><td class="firstcol" valign="top" width="30%" headers="new_feature_name"><p>Cross-segment state reuse</p></td>
<td class="cellrowborder" valign="top" width="70%" headers="new_feature_description"><p>This technology uses streams to save the automaton and composite rule states, and reuses the scratch workspace so that continuous data can be matched across any input segments.</p></td>
</tr>
</tbody>
</table>

## Related Documentation<a name="EN-US_TOPIC_0000002521728676"></a>

### Documentation<a name="EN-US_TOPIC_0000002521888670"></a>

<a name="table_supporting_documents"></a>

<table><tbody><tr id="row_supporting_documents_header"><th class="firstcol" valign="top" width="30%" id="document_name"><p><strong>Document</strong></p></th>
<th class="cellrowborder" valign="top" width="50%" id="document_description"><p><strong>Description</strong></p></th>
<th class="cellrowborder" valign="top" width="20%" id="document_delivery"><p><strong>Delivery Mode</strong></p></th>
</tr>
<tr id="row_document_release_notes"><td class="firstcol" valign="top" width="30%" headers="document_name"><p>Release Notes</p></td>
<td class="cellrowborder" valign="top" width="50%" headers="document_description"><p>Provides the Megascan version release information.</p></td>
<td class="cellrowborder" valign="top" width="20%" headers="document_delivery"><p>Open-source repository</p></td>
</tr>
<tr id="row_document_installation"><td class="firstcol" valign="top" width="30%" headers="document_name"><p>Installation Guide</p></td>
<td class="cellrowborder" valign="top" width="50%" headers="document_description"><p>Provides instructions for compiling, installing, and testing Megascan.</p></td>
<td class="cellrowborder" valign="top" width="20%" headers="document_delivery"><p>Open-source repository</p></td>
</tr>
<tr id="row_document_quick_start"><td class="firstcol" valign="top" width="30%" headers="document_name"><p>Quick Start</p></td>
<td class="cellrowborder" valign="top" width="50%" headers="document_description"><p>Provides examples of calling basic Megascan APIs.</p></td>
<td class="cellrowborder" valign="top" width="20%" headers="document_delivery"><p>Open-source repository</p></td>
</tr>
<tr id="row_document_api"><td class="firstcol" valign="top" width="30%" headers="document_name"><p>API Reference</p></td>
<td class="cellrowborder" valign="top" width="50%" headers="document_description"><p>Provides Megascan API definitions, parameters, and usage constraints.</p></td>
<td class="cellrowborder" valign="top" width="20%" headers="document_delivery"><p>Open-source repository</p></td>
</tr>
</tbody>
</table>

### Obtaining Documentation<a name="EN-US_TOPIC_0000002552888641"></a>

Visit the [open-source repository](https://gitcode.com/boostkit/Megascan) to view and download related documents.