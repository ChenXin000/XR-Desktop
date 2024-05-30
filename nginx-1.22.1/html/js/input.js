/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *   Copyright 2019 Google LLC
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */

/*global GamepadManager*/
/*eslint no-unused-vars: ["error", { "vars": "local" }]*/

class Input {
    /**
     * Input handling for WebRTC web app
     *
     * @constructor
     * @param {Element} [element]
     *    Video element to attach events to
     * @param {function} [send]
     *    Function used to send input events to server.
     */
    constructor(element, send) {
        // 键盘
        this.KEYEVENTF_KEYDOWN       = 0x0000   //按键按下
        this.KEYEVENTF_KEYUP         = 0x0002   //按键释放
        // 鼠标
        this.MOUSEEVENTF_MOVE        = 0x0001	//发生了移动。
        this.MOUSEEVENTF_LEFTDOWN    = 0x0002	//按下了左侧按钮。
        this.MOUSEEVENTF_LEFTUP      = 0x0004	//左按钮已释放。
        this.MOUSEEVENTF_RIGHTDOWN   = 0x0008	//按下了向右按钮。
        this.MOUSEEVENTF_RIGHTUP     = 0x0010	//右侧按钮已松开。
        this.MOUSEEVENTF_MIDDLEDOWN  = 0x0020	//按下中间按钮。
        this.MOUSEEVENTF_MIDDLEUP    = 0x0040	//中间按钮已释放。
        this.MOUSEEVENTF_WHEEL       = 0x0800	//如果鼠标有滚轮，则滚轮已移动。 移动量在 mouseData 中指定。
        this.MOUSEEVENTF_ABSOLUTE    = 0x8000	//dx 和 dy 成员包含规范化的绝对坐标。 如果未设置标志， dx和 dy 包含相对数据 (自上次报告的位置) 更改。

        this.MOUSE_DOWN_FLAGS = [0x0002,0x0020,0x0008] // 鼠标按键按下标志(左,中,右)键
        this.MOUSE_UP_FLAGS   = [0x0004,0x0040,0x0010] // 鼠标按键释放标志(左,中,右)键

        /**
         * @type {Element}
         */
        this.element = element;

        /**
         * @type {function}
         */
        this.send = send;
        /**
         * @type {Object}
         */
        this.m = null;
        /**
         * List of attached listeners, record keeping used to detach all.
         * @type {Array}
         */
        this.listeners = [];

        this.elementWidth = 0;
        this.elementHeight = 0;
        /**
         * @type {string}
         */
        this.opcode_ = '';
        /**
         * @type {timer}
         */
        this.clickTimer = null;

        this.prevX = 0;
        this.prevY = 0;

    }
    // 发送鼠标数据
    sendMouseData(flags = 0, x = 0, y = 0, wheel = 0) {
        let data = [this.opcode_, 'm', flags, parseInt(x), parseInt(y), wheel];
        this.send(data.join(","));
    }
    // 发送按键数据
    sendKeyData(flags = 0, key = 0) {
        let data = [this.opcode_, 'k', flags, key];
        this.send(data.join(','));
    }
    // 检查操作码
    checkOpcode(opc) {
        this.opcode_ = opc;
        let data = [this.opcode_, 'o', 'c'];
        this.send(data.join(','));
    }

    /**
     * Handles mouse button and motion events and sends them to WebRTC app.
     * @param {MouseEvent} event
     */
    _mouseButtonMovement(event) {
        this._windowMath();
        if(event.button < 0 || event.button > 2)
            return ;

        let flags = this.MOUSEEVENTF_MOVE;
        if(event.type !== 'mousemove') {
            if(event.type === 'mousedown') {
                flags |= this.MOUSE_DOWN_FLAGS[event.button];
                // CTRL-SHIFT-LeftButton 锁定鼠标
                if(event.button === 0 && event.ctrlKey && event.shiftKey) {
                    if (!!document.pointerLockElement) {
                        document.exitPointerLock(); // 指针被锁定时解锁
                      } else  event.target.requestPointerLock(); // 指针未被锁定时加锁
                    return;
                }
            } else { // mouseup
                flags |= this.MOUSE_UP_FLAGS[event.button];
            }
        }

        if(document.pointerLockElement) {
            this.sendMouseData(flags,event.movementX,event.movementY,0);
        } else {
            flags |= this.MOUSEEVENTF_ABSOLUTE;
            let x = this._clientToServerX(event.clientX);
            let y = this._clientToServerY(event.clientY);
            this.sendMouseData(flags,x,y,0);
        }
        event.preventDefault();
    }
    /**
     * Handles touch events and sends them to WebRTC app.
     * @param {TouchEvent} event
     */
    _touch(event) {
        this._windowMath();
        if(event.changedTouches.length > 1)
            return;
        
        let flags = this.MOUSEEVENTF_MOVE | this.MOUSEEVENTF_ABSOLUTE;
        let x = this._clientToServerX(event.changedTouches[0].clientX);
        let y = this._clientToServerY(event.changedTouches[0].clientY);

        if(event.type !== 'touchmove') {
            if (event.type === 'touchstart') {

                if(this.clickTimer) {
                    // 执行双击
                    if(Math.abs(x - this.prevX) < 655 && Math.abs(y - this.prevY) < 655) {
                        flags = 0;
                    }
                }
                this.prevX = x;
                this.prevY = y;
                
                flags |= this.MOUSEEVENTF_LEFTDOWN;
            } else { // touchend
                if(!this.clickTimer) {
                    this.clickTimer = setTimeout(()=>{ this.clickTimer = null; }, 180);
                }
                flags = this.MOUSEEVENTF_LEFTUP;
            }
        } else {
            event.preventDefault();
        }

        this.sendMouseData(flags,x,y,0);
    }

    /**
     * Handles mouse wheel events and sends them to WebRTC app.
     * @param {MouseWheelEvent} event
     */
    _mouseWheel(event) {
        this.sendMouseData(this.MOUSEEVENTF_WHEEL,0,0,-event.deltaY);
        event.preventDefault();
    }

    /**
     * Captures mouse context menu (right-click) event and prevents event propagation.
     * @param {MouseEvent} event
     */
    _contextMenu(event) {
        event.preventDefault();
    }

    /**
     * Captures keyboard events to detect pressing of CTRL-SHIFT hotkey.
     * @param {KeyboardEvent} event
     */
    _key(event) {
        // 字母统一转换成大写
        let key = (event.key.length ===  1) ? event.key.toLocaleUpperCase() : event.key
        let index = event.location > 1 ? 1 : 0; // 判断左右键和扩展键
        let codes = keyToScanCode[key];
        if(codes) {
            if(event.type === 'keydown') {
                this.sendKeyData(this.KEYEVENTF_KEYDOWN,codes[index]);
            } else {
                this.sendKeyData(this.KEYEVENTF_KEYUP,codes[index]);
            }
            event.preventDefault();
        }
    }
    /**
     * Captures display and video dimensions required for computing mouse pointer position.
     * This should be fired whenever the window size changes.
     */
    _windowMath() {
        const windowW = this.element.offsetWidth;
        const windowH = this.element.offsetHeight;
        // 判断元素窗口大小是否变化
        if(this.elementWidth === windowW && this.elementHeight === windowH)
            return ;
        this.elementWidth = windowW;
        this.elementHeight = windowH;

        const frameW = this.element.videoWidth;
        const frameH = this.element.videoHeight;

        const multi = Math.min(windowW / frameW, windowH / frameH);
        // 显示的视频画面宽高
        const vpWidth = frameW * multi;
        const vpHeight = (frameH * multi);

        this.m = {
            mouseMultiX: frameW / vpWidth,
            mouseMultiY: frameH / vpHeight,
            // 计算画面的黑边
            mouseOffsetX: Math.max((windowW - vpWidth) / 2.0, 0),
            mouseOffsetY: Math.max((windowH - vpHeight) / 2.0, 0),
            // windows 鼠标绝对坐标 (0，0) 映射到显示图面的左上角;坐标 (65535，65535) 映射到右下角。
            absoluteMultiX: 65535 / frameW,
            absoluteMultiY: 65535 / frameH,

            centerOffsetX: 0,
            centerOffsetY: 0,

            scrollX: window.scrollX,
            scrollY: window.scrollY,
            frameW,
            frameH,
        };
    }

    /**
     * Translates pointer position X based on current window math.
     * @param {Integer} clientX
     */
    _clientToServerX(clientX) {
        let serverX = Math.round((clientX - this.m.mouseOffsetX - this.m.centerOffsetX + this.m.scrollX) * this.m.mouseMultiX);

        serverX *= this.m.absoluteMultiX;
        if(serverX > 65535) serverX = 65535;
        else if(serverX < 0) serverX = 0;
        return serverX;
    }

    /**
     * Translates pointer position Y based on current window math.
     * @param {Integer} clientY
     */
    _clientToServerY(clientY) {
        let serverY = Math.round((clientY - this.m.mouseOffsetY - this.m.centerOffsetY + this.m.scrollY) * this.m.mouseMultiY);

        serverY *= this.m.absoluteMultiY;
        if(serverY > 65535) serverY = 65535;
        else if(serverY < 0) serverY = 0;
        return serverY;
    }

    /**
     * When fullscreen is entered, request keyboard and pointer lock.
     */
    _onFullscreenChange() {

        if (document.fullscreenElement !== null) {
            // Enter fullscreen
            if (!('ontouchstart' in window)) {
                this.requestKeyboardLock();
                this.element.requestPointerLock();
            }
        } else {
            document.exitPointerLock();
        }
    }
    
    /**
     * Attaches input event handles to docuemnt, window and element.
     */
    attach() {
        this.listeners.push(addListener(this.element, 'mousewheel', this._mouseWheel, this));
        this.listeners.push(addListener(this.element, 'contextmenu', this._contextMenu, this));
        this.listeners.push(addListener(window, 'fullscreenchange', this._onFullscreenChange, this));
        // this.listeners.push(addListener(document, 'pointerlockchange', this._pointerLock, this));
        this.listeners.push(addListener(window, 'keydown', this._key, this));
        this.listeners.push(addListener(window, 'keyup', this._key, this));

        if ('ontouchstart' in window) {
            this.listeners.push(addListener(this.element, 'touchstart', this._touch, this));
            this.listeners.push(addListener(this.element, 'touchend', this._touch, this));
            this.listeners.push(addListener(this.element, 'touchmove', this._touch, this));
        } else {
            this.listeners.push(addListener(this.element, 'mousemove', this._mouseButtonMovement, this));
            this.listeners.push(addListener(this.element, 'mousedown', this._mouseButtonMovement, this));
            this.listeners.push(addListener(this.element, 'mouseup', this._mouseButtonMovement, this));
        }
        // Adjust for scroll offset
        this.listeners.push(addListener(window, 'scroll', () => {
            this.m.scrollX = window.scrollX;
            this.m.scrollY = window.scrollY;
        }, this));

        this._windowMath();
    }

    detach() {
        removeListeners(this.listeners);
        document.exitPointerLock();
    }
    /**
     * Request keyboard lock, must be in fullscreen mode to work.
     */
    requestKeyboardLock() {
        // event codes: https://www.w3.org/TR/uievents-code/#key-alphanumeric-writing-system
        const keys = [
            "AltLeft",
            "AltRight",
            "Tab",
            "Escape",
            "ContextMenu",
            "MetaLeft",
            "MetaRight"
        ];
        console.log("requesting keyboard lock");
        navigator.keyboard.lock(keys).then(
            () => {
                console.log("keyboard lock success");
            }
        ).catch(
            (e) => {
                console.log("keyboard lock failed: ", e);
            }
        )
    }
}

/**
 * Helper function to keep track of attached event listeners.
 * @param {Object} obj
 * @param {string} name
 * @param {function} func
 * @param {Object} ctx
 */
function addListener(obj, name, func, ctx) {
    const newFunc = ctx ? func.bind(ctx) : func;
    obj.addEventListener(name, newFunc);

    return [obj, name, newFunc];
}

/**
 * Helper function to remove all attached event listeners.
 * @param {Array} listeners
 */
function removeListeners(listeners) {
    for (const listener of listeners)
        listener[0].removeEventListener(listener[1], listener[2]);
}
// 按键转 Windows 扫描码
const keyToScanCode = {
    'F1':  [0x003B],
    'F2':  [0x003C],
    'F3':  [0x003D],
    'F4':  [0x003E],
    'F5':  [0x003F],
    'F6':  [0x0040],
    'F7':  [0x0041],
    'F8':  [0x0042],
    'F9':  [0x0043],
    'F10': [0x0044],
    'F11': [0x0057],
    'F12': [0x0058],
    'F13': [0x0064],
    'F14': [0x0065],
    'F15': [0x0066],
    'F16': [0x0067],
    'F17': [0x0068],
    'F18': [0x0069],
    'F19': [0x006A],
    'F20': [0x006B],
    'F21': [0x006C],
    'F22': [0x006D],
    'F23': [0x006E],
    'F24': [0x0076],
    '`': [0x0029],        '~': [0x0029],
    '1': [0x0002,0x004F], '!': [0x0002],
    '2': [0x0003,0x0050], '@': [0x0003],
    '3': [0x0004,0x0051], '#': [0x0004],
    '4': [0x0005,0x004B], '$': [0x0005],
    '5': [0x0006,0x004C], '%': [0x0006],
    '6': [0x0007,0x004D], '^': [0x0007],
    '7': [0x0008,0x0047], '&': [0x0008],
    '8': [0x0009,0x0048], '*': [0x0009,0x0037],
    '9': [0x000A,0x0049], '(': [0x000A],
    '0': [0x000B,0x0052], ')': [0x000B],
    '-': [0x000C,0x004A], '_': [0x000C],
    '=': [0x000D,0x0059], '+': [0x000D,0x004E],
    'A': [0x001E],
    'B': [0x0030],
    'C': [0x002E],
    'D': [0x0020],
    'E': [0x0012],
    'F': [0x0021],
    'G': [0x0022],
    'H': [0x0023],
    'I': [0x0017],
    'J': [0x0024],
    'K': [0x0025],
    'L': [0x0026],
    'M': [0x0032],
    'N': [0x0031],
    'O': [0x0018],
    'P': [0x0019],
    'Q': [0x0010],
    'R': [0x0013],
    'S': [0x001F],
    'T': [0x0014],
    'U': [0x0016],
    'V': [0x002F],
    'W': [0x0011],
    'X': [0x002D],
    'Y': [0x0015],
    'Z': [0x002C],
    '[': [0x001A], '{': [0x001A],
    ']': [0x001B], '}': [0x001B],
    '|': [0x002B], '\\': [0x002B],
    ';': [0x0027], ':': [0x0027],
    "'": [0x0028], '"': [0x0028],
    ',': [0x0033], '<': [0x0033],
    '.': [0x0034,0x0053], '>': [0x0034],
    '/': [0x0035,0xE035], '?': [0x0035],
    ' ': [0x0039],
    'Escape'      : [0x0001],
    'Backspace'   : [0x000E],
    'Tab'         : [0x000F],
    'CapsLock'    : [0x003A],
    'Enter'       : [0x001C,0xE01C],
    'Shift'       : [0x002A,0x0036],
    'Control'     : [0x001D,0xE01D],
    'Meta'        : [0xE05B,0xE05C],
    'Alt'         : [0x0038,0xE038],
    'PrintScreen' : [0xE037],
    'ScrollLock'  : [0x0046],
    'Pause'       : [0x00E1],
    'Insert'      : [0xE052,0x0052],
    'Home'        : [0xE047,0x0047],
    'PageUp'      : [0xE049,0x0049],
    'Delete'      : [0xE053],
    'End'         : [0xE04F,0x004F],
    'PageDown'    : [0xE051,0x0051],
    'ArrowRight'  : [0xE04D,0x004D],
    'ArrowLeft'   : [0xE04B,0x004B],
    'ArrowDown'   : [0xE050,0x0050],
    'ArrowUp'     : [0xE048,0x0048],
    'NumLock'     : [0x0045],
};