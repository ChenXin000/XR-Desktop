class Debug {
    constructor(element) {
        this.element = element;
        this.pr = 0;
    }
    // 在元素上打印调试信息
    print(tip, text, color = "#fff") {
        // white-space: pre-wrap;word-break: break-all;
        let html = `<pre style='margin: 2px;  color: ${color};'>[${tip}]: ${text}</pre>`;
        this.element.innerHTML += html;
        // this.element.scrollTop = this.element.scrollHeight;
    }
    // 在元素上打印错误信息
    error(tip, text) {
        this.print(tip, text, "red");
        this.element.scrollTop = this.element.scrollHeight;
    }
    // 在元素上打印警告信息
    warning(tip, text) {
        this.print(tip, text, "yellow");
        this.element.scrollTop = this.element.scrollHeight;
    }
    // 在元素上打印成功信息
    success(tip, text) {
        this.print(tip, text, "green");
        // this.element.scrollTo(0,this.element.scrollHeight)
        this.element.scrollTop = this.element.scrollHeight;
    }
}

// export {MyDebug};