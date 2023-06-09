# Scene Descriptor

The scene descriptor is a [JSON file](#about-json) that contains information about the camera, the player and things that you can build and place in the scene called **[rigid bodies](#rigid-body)**. The scene descriptor recognizes these members to create the scene: **[camera](#camera)**, **[player](#player)**, **[macros](#macros)**, and **[scene](#scene)**. Other members of the scene descriptor are ignored and can be used as annotations.

## **rigid body**

A rigid body represents a real-world object that is being simulated in software using physics and mathematics and visualized by the computer's graphics system and the software called OpenGL. A rigid body comprises a simple or **[compound shape](shape_desc.md#compound-shape)**, a 3D vector for position, a quaternion for orientation, and mass of the object. 

## **3D vector**

A *3D vector* (x, y, z) is represented as a JSON array of three floating numbers **[x, y, z]**. For example, (1, 0, 0) is a unit vector whose length is 1 on the x-axis.

## **Quaternion**

A quaternion consists of a *3D vector* (axis for rotation) and a scalar (angle in degrees for rotation). It is represented as a JSON array of four (4) floating point numbers **[x, y, z, angle]**. For example [0, 1, 0, 90] represents a 90 degree (counterclock-wise) rotation about the +y axis.

## **camera**

A JSON object that has three members: **eye**, **target** and **follow**. 

* **eye** is a *3D vector* describing the camera's position. If the camera follows the player this vector is in the player's local frame otherwise world frame. This is a required member.

* **target** is a *3D vector* representing where the camera is looking at and uses the same reference frame as the **eye**. This is an optional member. If missing, the camera will look in +z direction if not following the player or at the player vehicle when following.

* **follow** is a boolean value. If it is true AND a player is present, the camera follows the player vehicle. This is an optional member. The default is **false** when it is missing.

Example:

```json
"camera": {
    "eye": [10, 5, 10],
    "target": [0, 0, 0],
    "follow": false
}
```

## **player**

A JSON object that describes the player vehicle. It recognizes two members **vehicle** and **origin**. Both members are required.

* **vehicle** is a string value. It can be either **tank** or **V150**.
* **origin** is a *3D vector* describing the player vehicle's position in world frame.

Example:

```json
"player": {
    "vehicle": "tank",
    "origin": [0, 1.5, 30]
}
```

## **macros**

A JSON object that contains one or more named **[shape descriptors](shape_desc.md)** that can be referened elsewhere in the file. 

## **scene**

The scene descriptor is a JSON array that contains one or more **rigid body descriptors**. A rigid body descriptor is a JSON object that has these members: **shape**, **origin**, **rotation**, and **mass**.

* **shape** is either a [shape descriptor](shape_desc.md) or a string value that is the name of a shape descriptor in **macros**.

* **origin** is a *3D vector* describing the rigid body's position in world frame.

* **rotation** is a *quaternion* describing the orientation of the rigid body in world frame. This member is optional. If missing, no rotation is applied to the rigid body.

* **mass** is a JSON floating point number. It is optional. If mass is missing or has a value of 0, the rigid body is static. A static rigid body cannot move.

Example of a **rigid body descriptor**:

```json
{  
    "shape": "axes",
    "origin": [0, 0, 0],
    "rotation": [0, 1, 0, 0],
    "mass": 0
}
```
## **About JSON**

A json file contains one object at the very top level (root). An object is enclosed by **{ }** and contains one or more **members** separated by a comma (**,**). Members are in the form of **name:value** where name is a string enclosed by **" "**, and value can be a number, a string, a boolean, another object or an **ordered array** of values enclosed by **[ ]**. Order of members is not important; however names have to be unique within the same object.

Example of a JSON object:

```json
{
    "name": "value",
    "array": [1.0, 2, "car", true, {"color":"green"}, null],
    "object": {"fruit":"orange"}
}
```

Some useful links:

* [JSON](https://www.json.org/): Introduction to JSON 
* [JSON lint](https://jsonlint.com/): JSON validator and reformatter
