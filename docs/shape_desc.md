# Shape Descriptor

The shape descriptor is a JSON object that consists of three members: **[kind](#kind)**, **[dimension](#dimension)** and **[textures](#textures)**. 

```json
{
    "kind": "...",
    "dimension": [...],
    "textures": [...]
}
```

See [examples](examples.json) here.

## **kind**

A string value that can be one of following shapes: **ground**, **box**, **sphere**, **capsule**, **cone**, **cylinder**, **pyramid**, **wedge**, **gear** and [**compound**](#compound-shape).

## **dimension** 
A JSON array that contains shape specific parameters.

|kind|dimension|
|----|---------|
|ground| `width, length`|
|box|`half_width, half_height, half_length`|
|sphere|`radius`|
|capsule|`radius, height`|
|cone|`radius, height`|
|cylinder|`radius, half_height`|
|pyramid|`half_width, half_height, half_length`|
|wedge|`half_width, half_height, half_length, half_top_edge_length`|
|gear|`radius, half_thickness, num_teeth`|
|compound|`"child":[{`**`shape descriptor`**`, origin, rotation},...]`|

* `all numbers are floating point numbers except that num_teeth for gear shape is an integer`
* `width(x), height(y), length(z): extend along the designated axis`
* `half_width(x), half_height(y), half_length(z): half extend along designated axis on both sides of the shape center`
* `half_thickness of the gear shape is along the y axis`
* `The wedge shape has a rectangular base parallel to the x-z plane, the top edge is along the z-axis`

## **compound shape**

A compound shape allows multiple shapes to be combined to form a more complex shape. Unlike simple shape descriptors, a compound shape descriptor has a JSON array **(child)** containing objects representing child shapes. Each child shape has a shape descriptot, a 3D vector **(origin)** for its position relative to the center of the compound shape, and a quaternion **(rotation)** describing its orientation. A compound shape can be a child shape of another compound shape to form nested commpound shapes.

```json
{
    "kind": "compound",
    "child": [{
        "shape": {<shape descriptor>} or "<name in macros>",
        "origin": [x, y, z],
        "rotation": [x, y, z, r] 
        },
        ...
    ]
}
```

## **textures**

A JSON array containing one or more texture objects. A texture object has the following form:

```json
{"<texture type>": [<parameters>], "repeat": n}
```

|texture type|parameters|
|------------|----------|
|horizontal_stripes|`width, color_1, color_2`|
|vertical_stripes|`height, color_1, color_2`|
|diagonal_stripes|`width, height, style, color_1, color_2`|
|checker_board|`width , height, color_1, color_2`|
|color|`HTML hex color code or color name`|
|file|`pathname to an image file (.jpg, .png, etc.)`|

* **color** can be either HTML hex color code `("#DEB887")` or color name `("red")`
* **diagonal_stripes** has these styles:
    * `0: top right to bottom left`
    * `1: top left to bottom right`
    * `2: top center to bottom left and right`
    * `3: bottom center to top left and right`
* **repeat** is an optional integer value that has these meanings:
    * `not present: no repeat`
    * `0: default texture used for faces with no textures`
    * `n: repeat this texture n times`
