/*
 * Copyright (c) 2023 Isaac Chou
 * 
 * This software is licensed under the MIT License that can be 
 * found in the LICENSE file at the top of the source tree
 */
const mat4 = glMatrix.mat4;
const vec3 = glMatrix.vec3;

const texture_map = new Map();
const keyboard = new Set();
const mouse = new Set();
const key_map = new Map([["Escape",256],["Enter",257],["ArrowRight",262],["ArrowLeft",263],
                        ["ArrowDown",264],["ArrowUp",265],["ShiftLeft",340],["ShiftRight",344]]);

class Renderer 
{
  constructor(gl) {
    this.gl = gl;
    this.shaderProgram = this.initShaderProgram();
    this.gl.useProgram(this.shaderProgram);
    this.shape_map = new Map();
  }

  setup() {
    const gl = this.gl;
    const shaderProgram = this.shaderProgram;

    const projection = mat4.create();
    mat4.perspective(projection, glMatrix.glMatrix.toRadian(60.0), 1024.0/576.0, 0.1, 600.0);
    gl.uniformMatrix4fv(gl.getUniformLocation(shaderProgram, "projection"), false, new Float32Array(projection));

    // a directional light vector pointing from the light source
    const light_direction = vec3.create();
    vec3.normalize(light_direction, [-1, -3, 0]);
    const light_ambient = 0.6;
    gl.uniform1f(gl.getUniformLocation(shaderProgram, "light.ambient"), light_ambient);
    gl.uniform3fv(gl.getUniformLocation(shaderProgram, "light.direction"), light_direction);
    
    gl.enable(gl.DEPTH_TEST);

    // reder one frame
    gl.clearColor(0.2, 0.3, 0.3, 1.0);
    gl.clearDepth(1.0);
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);

    // setup camera transform
    const view = mat4.create();
    mat4.lookAt(view, [30, 7.5, -170], [30, 5, -150], [0, 1, 0]);
    gl.uniformMatrix4fv(gl.getUniformLocation(shaderProgram, "view"), false, new Float32Array(view));
  }

  draw() {
    const gl = this.gl;
    gl.clearColor(0.2, 0.3, 0.3, 1.0);
    gl.clearDepth(1.0);
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
    for (let shape of this.shape_map.values()) {
      shape.draw(this.shaderProgram, shape.trans);
    }
  }

  add_texture(id, width, height, image) {
    const gl = this.gl;
    const txtr = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, txtr);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.REPEAT);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.REPEAT);        
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGB, width, height, 0, gl.RGB, gl.UNSIGNED_BYTE, image);
    texture_map.set(id, txtr);
  }

  add_shape(shape_id, descriptor) {
    this.shape_map.set(shape_id, new Shape(this.gl, descriptor));
  }

  update_shape(shape_id, trans) {
    if(this.shape_map.has(shape_id)) {
      this.shape_map.get(shape_id).trans = trans;
    }
  }

  remove_shape(shape_id) {
    if(this.shape_map.has(shape_id)) {
      this.shape_map.delete(shape_id);
    }
  }

  initShaderProgram()
  { 
    const gl = this.gl;

    // Vertex shader
    const vsSource = `#version 300 es
      layout(location = 0) in vec3 pos;
      layout(location = 1) in vec2 t;
      layout(location = 2) in vec3 n;

      out vec3 normal;
      out vec2 txtr_pos;

      uniform mat4 model;
      uniform mat4 view;
      uniform mat4 projection;

      void main()
      {
        normal = mat3(transpose(inverse(model))) * n;
        txtr_pos = t;
        gl_Position = projection * view * model * vec4(pos, 1.0);
      }    
    `;

    // Fragment shader
    const fsSource = `#version 300 es
      precision mediump float;

      struct Light {
        float ambient;
        vec3 direction;
      };
      in vec3 normal;
      in vec2 txtr_pos;
      out vec4 clr;

      uniform sampler2D txtr;
      uniform Light light;

      void main()
      {
        float light = max(dot(normal, -light.direction), 0.0) + light.ambient;
        clr = vec4(light * texture(txtr, txtr_pos).rgb, 1.0);
      }  
    `;
    const vertexShader = this.loadShader(gl.VERTEX_SHADER, vsSource);
    const fragmentShader = this.loadShader(gl.FRAGMENT_SHADER, fsSource);

    const shaderProgram = gl.createProgram();
    gl.attachShader(shaderProgram, vertexShader);
    gl.attachShader(shaderProgram, fragmentShader);
    gl.linkProgram(shaderProgram);
    if (!gl.getProgramParameter(shaderProgram, gl.LINK_STATUS)) {
      alert(`Failed to link program: ${gl.getProgramInfoLog(shaderProgram)}`);
      return null;
    }
    return shaderProgram;
  }

  loadShader(type, source)
  {
    const gl = this.gl;
    const shader = gl.createShader(type);
    gl.shaderSource(shader, source);
    gl.compileShader(shader);
    if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
      alert(`Failed to compile shader: ${gl.getShaderInfoLog(shader)}`);
      gl.deleteShader(shader);
      return null;
    }
    return shader;
  }
}

class Shape
{
  constructor(gl, d) {
    this.gl = gl;    
    this.trans = d.trans;
    if (Object.hasOwn(d, 'default_texture')) {
      this.default_texture = texture_map.get(d.default_texture);
    }
    
    if (Object.hasOwn(d, 'child')) {
      // compound shape
      this.child = new Array();
      for (let s of d.child) {
        this.child.push(new Shape(gl, s));
      }
      return;
    } else {
      // simple shape
      this.mesh = d.mesh;
      this.face_index = d.face_index;
      this.textures = Object.hasOwn(d, "textures") ? d.textures : [];
    }

    // setup shape vertices
    this.vao = gl.createVertexArray();
    gl.bindVertexArray(this.vao);

    this.buffer = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, this.buffer);

    const mesh = this.mesh;
    const num_vertices = mesh.length / 5;
    const normal = new Array();
    for (let n = 0; n < num_vertices; n += 3) {
      let i = n * 5;
      //const mesh = shape.mesh;
      const p1 = vec3.fromValues(mesh[i++], mesh[i++], mesh[i++]); i += 2;
      const p2 = vec3.fromValues(mesh[i++], mesh[i++], mesh[i++]); i += 2;
      const p3 = vec3.fromValues(mesh[i++], mesh[i++], mesh[i++]);

      // one normal vector for each vertex!!!
      const norm = vec3.create();
      vec3.subtract(p2, p2, p1);
      vec3.subtract(p3, p3, p1);
      vec3.normalize(norm, vec3.cross(norm, p2, p3));
      normal.push(norm[0], norm[1], norm[2]);
      normal.push(norm[0], norm[1], norm[2]);
      normal.push(norm[0], norm[1], norm[2]);
    }
    gl.bufferData(gl.ARRAY_BUFFER, num_vertices * (5 + 3) * 4, gl.STATIC_DRAW);
    gl.bufferSubData(gl.ARRAY_BUFFER, 0, new Float32Array(this.mesh));
    gl.bufferSubData(gl.ARRAY_BUFFER, num_vertices * 5 * 4, new Float32Array(normal));

    // pos buffer (location = 0 in vertex shader)
    gl.vertexAttribPointer(0, 3, gl.FLOAT, false, 5 * 4, 0);
    gl.enableVertexAttribArray(0);

    // texture coordinates (location = 1)
    gl.vertexAttribPointer(1, 2, gl.FLOAT, false, 5 * 4, 3 * 4);
    gl.enableVertexAttribArray(1);

    // normals (location = 2)
    gl.vertexAttribPointer(2, 3, gl.FLOAT, false, 3 * 4, num_vertices * 5 * 4);
    gl.enableVertexAttribArray(2);
  }

  draw(shaderProgram, model) {
    const gl = this.gl;
    if (Object.hasOwn(this, 'child')) {
      for (let s of this.child) {
        const m = mat4.create();
        mat4.multiply(m, model, s.trans)
        s.draw(shaderProgram, m);
      }    
    } 
    else 
    { // draw a simple shape
      gl.uniformMatrix4fv(gl.getUniformLocation(shaderProgram, "model"), false, new Float32Array(model));
      gl.bindVertexArray(this.vao);
      const num_vertices = this.mesh.length / 5;
      for (let i = 0; i < this.face_index.length; i++)
      {          
        if (!Object.hasOwn(this, "default_texture") && i >= this.textures.length) {
          // skip the face if no texture specified
          // no wire-frame drawing mode support in WebGL
          continue;
        }        
        const index = this.face_index[i];
        const n = (i == this.face_index.length - 1) ? num_vertices - index : this.face_index[i + 1] - index;
        const txtr = (i < this.textures.length) ? texture_map.get(this.textures[i]) : this.default_texture;
        gl.activeTexture(gl.TEXTURE0);
        gl.bindTexture(gl.TEXTURE_2D, txtr);    
        gl.uniform1i(gl.getUniformLocation(shaderProgram, "txtr"), 0);
        gl.drawArrays(gl.TRIANGLES, index, n);
      }
    }
  }
}

const game_client = new (class {
  constructor() {
    const canvas = document.querySelector("canvas");
    const gl = canvas.getContext("webgl2");
    if (gl === null) {
      alert("Failed to initialize WebGL");
      return;
    }
    this.renderer = new Renderer(gl);
    this.renderer.setup();

    const hostname = location.hostname;
    const port = "9001"; // change this if the game server is listening on a different port
    const url = hostname + ":" + port
    this.socket = this.connect(url);
    this.init_ctrls();
  }

  init_ctrls() {  
    document.body.onkeydown = function(e){
      if(key_map.has(e.code)) {
        keyboard.add(key_map.get(e.code));
      }
    };
  
    document.body.onkeyup = function(e){
      if(key_map.has(e.code)) {
        keyboard.delete(key_map.get(e.code));
      }
    };
  
    document.body.onmousedown = function(e){
      if (e.button == 0) mouse.add(0);
      else if (e.button == 2) mouse.add(1);
    }
    
    document.body.onmouseup = function(e){
      if (e.button == 0) mouse.delete(0);
      else if (e.button == 2) mouse.delete(1);
    }
  
    document.body.onmouseleave = function(e){
      mouse.clear();
    }  
  }

  connect(url) {
    const socket = new WebSocket("ws://" + url);
    socket.onerror = (event) => { alert("Failed to connect to game server @ " + url); }
    socket.onopen = (event) => {};  
    socket.onmessage = (event) => {
      const n = event.data.length;
      const msg = JSON.parse(event.data);
      switch (msg.cmd) {
        // initial setup messages
        case "set_player_id":
          break;
        case "setup_camera":
          break;
        case "add_texture":
          const image = Uint8Array.from(atob(msg.data), (c) => c.charCodeAt(0));
          this.renderer.add_texture(msg.id, msg.width, msg.height, image);
          break;
        
        // messages in an update cycle
        case "get_controller":
          const keys = [];
          for(let key of keyboard.values()) {
            keys.push(key);
          }
          const buttons = [];
          for(let button of mouse.values()) {
            buttons.push(button);
          }
          const ctlr = {
            "keyboard": keys,
            "mouse": buttons,
            "cursor_cur_pos": [0,0],
            "cursor_last_pos": [0,0],
            "cursor_scroll_pos": [0,0]
          };
          socket.send(JSON.stringify(ctlr));
          break;
        case "set_player_transform":
          break;
        case "add_shape":
          this.renderer.add_shape(msg.shape_id, msg.descriptor);
          break;
        case "update_shape":
          this.renderer.update_shape(msg.shape_id, msg.trans);
          break;
        case "remove_shape":
          this.renderer.remove_shape(msg.shape_id);
          break;      
        case "end_update":
          socket.send(JSON.stringify({"continue": true}));
          // reder one frame
          this.renderer.draw();
          break;

        // terminating message
        case "end":
          break; 
      }
    };
    return socket;
  }
});
