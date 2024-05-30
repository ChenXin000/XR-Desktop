let infoIndex = 0 // 悬浮按钮显示的信息
const main_container = document.getElementById("main-container");
const videoElement = document.getElementById("video-stream");
const audioElement = document.getElementById("audio-stream");

const refreshBut = document.getElementById("refresh-but");
const audioBut = document.getElementById("audio-but");
const debugBut = document.getElementById("debug-but");
const operateBut = document.getElementById("operate-but");
const fullscreenBut = document.getElementById("fullscreen-but");
const hoverBut = document.getElementById("hover-but");

const optInputBox = document.getElementById("opt-input");

const info_con_st = document.getElementById("info-con-st");
const info_con_ty = document.getElementById("info-con-ty");
const info_pac_re = document.getElementById("info-pac-re");
const info_pac_lo = document.getElementById("info-pac-lo");
const info_byt_re = document.getElementById("info-byt-re");
const info_byt_se = document.getElementById("info-byt-se");

const vids_lat = document.getElementById("vids-lat");
const vids_typ = document.getElementById("vids-typ");
// let vids_enc = document.getElementById("vids-enc");
const vids_dec = document.getElementById("vids-dec");
const vids_fra = document.getElementById("vids-fra");
const vids_bit = document.getElementById("vids-bit");
const vids_arb = document.getElementById("vids-arb");

const auds_lat = document.getElementById("auds-lat");
const auds_cod = document.getElementById("auds-cod");
const auds_bit = document.getElementById("auds-bit");

const media_container = document.getElementById("media-container");
const video_container = document.getElementById("video-container");
const control_container = document.getElementById("control-container");

const control_buts = document.getElementById("control-buts");
const input_box = document.getElementById("input-box");

const debug_container = document.getElementById("debug-container");
const debug_text = document.getElementById("debug-text");
const info_text = document.getElementById("info-text");

const select_container = document.getElementById("select-container");
const av_sync_but = document.getElementById("av-sync-but");
const start_but = document.getElementById("start-but");
const loading_container = document.getElementById("loading-container");

const connecting_anim = document.getElementById("connecting-anim");
// 设置按钮开启或关闭状态的样式
function setButStyle(but, state, image) {
    but.style.backgroundColor = state ? "#2189ff" : "";
    but.style.backgroundImage = "url('../image/" + image + "')";
}
// 初始化按钮状态
function initButs() {
    setButStyle(refreshBut, true, "refresh.svg");
    setButStyle(audioBut, false, "audio-off.svg");
    setButStyle(debugBut, true, "debug-on.svg");
    setButStyle(operateBut, false, "ctrl-off.svg");
    setButStyle(fullscreenBut, false, "fullscreen-off.svg");
}

video_container.addEventListener("click", ()=> { showControlBox(false); })
control_container.addEventListener("click", (e) => { e.stopPropagation(); })
// 设置按钮点击状态
refreshBut.addEventListener("click", () => { location.reload(false); })
audioBut.addEventListener("click", () => { openAudio(audioBut.style.backgroundColor === ""); })
debugBut.addEventListener("click", () => { showDebugBox(debugBut.style.backgroundColor === ""); })
// operateBut.addEventListener("click", () => { showInputBox(operateBut.style.backgroundColor === "");})
fullscreenBut.addEventListener("click", () => { openFullscreen(fullscreenBut.style.backgroundColor === "");})
av_sync_but.addEventListener("click",() => { switchAudSync(av_sync_but.style.backgroundColor === ""); })
start_but.addEventListener("click",() => { showMediaContainer(true); })
select_container.addEventListener("click",(e) => { e.stopPropagation(); })
optInputBox.addEventListener("input",(e) => { inputCheck(e); });
// 输入检查
function inputCheck(e) {
    if(!e.data) return ;
    if(optInputBox.value.length > 6)
        optInputBox.value = optInputBox.value.substring(0,6);
    if( (e.data < 'a' || e.data > 'z') && (e.data < 'A' || e.data > 'Z')) {
        optInputBox.value = optInputBox.value.slice(0,-1);
    }
}
// 切换音频同步按钮状态
function switchAudSync(state) {
    if(state) {
        av_sync_but.innerText = "音视频同步：开";
        av_sync_but.style.backgroundColor = "#2189ff"
    } else {
        av_sync_but.innerText = "音视频同步：关";
        av_sync_but.style.backgroundColor = ""
    }
}
// 是否播放音频
function openAudio(open) {
    if(open) {
        audioElement.muted = false;
        videoElement.muted = false;
        setButStyle(audioBut, true, "audio-on.svg");
    } else {
        audioElement.muted = true;
        videoElement.muted = true;
        setButStyle(audioBut, false, "audio-off.svg");
    }
}
// 是否显示debug文本框
function showDebugBox(show) {
    if(show) {
        video_container.style.height = "65%";
        debug_container.style.height = "35%";
        setButStyle(debugBut, true, "debug-on.svg");
    } else {
        video_container.style.height = "100%";
        debug_container.style.height = "0";
        setButStyle(debugBut, false, "debug-off.svg");
    }
}
// 是否显示输入框
function showInputBox(show) {
    if(show && control_container.style.opacity === "") {
        control_buts.style.height = "0";
        control_buts.style.opacity = "0";
        input_box.style.opacity = "1";
        optInputBox.disabled = false;
        // 动画结束后自动聚焦输入框
        setTimeout(()=>{ optInputBox.focus(); },120);
    } else {
        input_box.style.opacity = "0";
        control_buts.style.height = "100%"
        control_buts.style.opacity = "1";
        optInputBox.disabled = true;
        optInputBox.value = "";
    }
}
// 切换远程操作按钮状态
function switchRemotingBut(state) {
    if(state) {
        showInputBox(false);
        setButStyle(operateBut, true, "ctrl-on.svg");
    } else {
        setButStyle(operateBut, false, "ctrl-off.svg");
    }
}
// 是否开启全屏
function openFullscreen(open) {
    if(document.fullscreenElement) {
        document.exitFullscreen();
    } else if(open) {
        media_container.requestFullscreen();
    }
}
// 监听全屏变化
window.addEventListener("fullscreenchange",(event)=>{
    if(document.fullscreenElement) {
        setButStyle(fullscreenBut, true, "fullscreen-on.svg");
        showDebugBox(false);
    } else {
        setButStyle(fullscreenBut, false, "fullscreen-off.svg");
    }
})
// 是否显示控制按钮
function showControlBox(show) {
    if(show) {
        showInputBox(false);
        control_container.style.height = "80px";
        control_container.style.opacity = "";
    } else {
        control_container.style.height = "0";
        control_container.style.opacity = "0";
    }
}
// 显示详细信息页面
function showInfoText(show) {
    if(show) {
        info_text.style.opacity = "1"
        debug_text.style.height = "0";
        debug_text.style.opacity = "0";
    } else {
        info_text.style.opacity = "0"
        debug_text.style.height = "100%";
        debug_text.style.opacity = "1";
    }
}
debug_text.addEventListener("click",(e) => { showInfoText(true); })
info_text.addEventListener("click",(e) => { showInfoText(false); })
// 是否显示媒体页面
function showMediaContainer(show) {
    if(show) select_container.style.display = "none";
    else select_container.style.display = "";
}
// 是否显示主页面
function showMainContainer(show) {
    if(show) {
        main_container.style.display = "";
        connecting_anim.style.display = "none";
    } else {
        main_container.style.display = "none";
        connecting_anim.style.display = "";
    }
}
// 将统计数据显示到元素
function showStats(state) {
    info_con_st.innerText = state
    info_con_ty.innerText = connectionStatType;
    info_pac_re.innerText = connectionPacketsReceived
    info_pac_lo.innerText = connectionPacketsLost
    info_byt_re.innerText = connectionBytesReceived
    info_byt_se.innerText = connectionBytesSent

    vids_lat.innerText = connectionVideoLatency + ' ms'
    vids_typ.innerText = connectionCodec + ' (' + connectionResolution + ')'
    // vids_enc.innerText = encoderName
    vids_dec.innerText = connectionVideoDecoder
    vids_fra.innerText = connectionFrameRate + ' fps'
    vids_bit.innerText = connectionVideoBitrate + ' mbps'
    vids_arb.innerText = connectionAvailableBandwidth

    auds_lat.innerText = connectionAudioLatency + ' ms'
    auds_cod.innerText = connectionAudioCodecName
    auds_bit.innerText = connectionAudioBitrate + ' kbps'

    // 在悬浮按钮上显示内容
    let info = connectionVideoLatency
    if(infoIndex === 1) {
        info = connectionFrameRate
        if(hoverBut.style.color === '') info = 'F'
        hoverBut.style.color = '#0f0'
    } else {
        if(hoverBut.style.color !== '') info = 'M'
        hoverBut.style.color = ''
    }
    if(info) {
        hoverBut.innerHTML = `<b>${info}</b>`;
    }
}

//使 DIV 元素可以拖动：
function dragElement(elmnt, onClick) {
    let inX = 0, inY = 0;
    let pos1 = 0, pos2 = 0, pos3 = 0, pos4 = 0;
    let timeOut = null;
    let isMove = false;
    if ('ontouchstart' in window)
        elmnt.ontouchstart = dragMouseDown;
    else elmnt.onmousedown = dragMouseDown;
    resetPos();

    window.addEventListener("resize", resetPos)
    function resetPos() {
        elmnt.style.left = (window.innerWidth - elmnt.clientWidth - 5) + 'px';
        elmnt.style.top = '5px'
        elmnt.style.opacity = '0.5'
    }
    // 按下事件
    function dragMouseDown(e) {
        e = e || window.event;
        e.stopPropagation();
        // 在启动时获取鼠标光标位置：
        if (e.clientX || e.clientY) {
            inX = pos3 = e.clientX;
            inY = pos4 = e.clientY;
            document.onmouseup = closeDragElement;
            // 当光标移动时调用一个函数：
            document.onmousemove = elementDrag;
        } else {
            inX = pos3 = e.changedTouches[0].clientX
            inY = pos4 = e.changedTouches[0].clientY
            elmnt.ontouchend = closeDragElement;
            elmnt.ontouchmove = elementDrag;
        }

        elmnt.style.opacity = '1'
        if (timeOut !== null) {
            clearTimeout(timeOut)
            timeOut = null;
        }
    }
    // 移动事件
    function elementDrag(e) {
        e = e || window.event;
        e.stopPropagation();
        // 计算新的光标位置：
        if (e.clientX || e.clientY) {
            pos1 = pos3 - e.clientX;
            pos2 = pos4 - e.clientY;
            pos3 = e.clientX;
            pos4 = e.clientY;
        } else {
            pos1 = pos3 - e.changedTouches[0].clientX
            pos2 = pos4 - e.changedTouches[0].clientY
            pos3 = e.changedTouches[0].clientX
            pos4 = e.changedTouches[0].clientY
        }
        // 设置元素的新位置：限制在窗口内
        let y = (elmnt.offsetTop - pos2)
        if (y < 0) { y = 0; }
        else if (y + elmnt.clientHeight >= window.innerHeight) {
            y = window.innerHeight - elmnt.clientHeight;
        }
        let x = (elmnt.offsetLeft - pos1)
        if (x < 0) { x = 0; }
        else if (x + elmnt.clientWidth >= window.innerWidth) {
            x = window.innerWidth - elmnt.clientWidth;
        }

        elmnt.style.top = y + 'px';
        elmnt.style.left = x + 'px';
        isMove = true;
    }
    // 抬起事件
    function closeDragElement(e) {
        /* 释放鼠标按钮时停止移动：*/
        elmnt.ontouchend = null;
        elmnt.ontouchmove = null;
        document.onmouseup = null;
        document.onmousemove = null;
        if (!isMove) {
            if (onClick) onClick(e);
        }
        // 5秒后设置元素半透明   
        timeOut = setTimeout((e) => {
            elmnt.style.opacity = '0.5'
            timeOut = null;
        }, 5000)

        isMove = false;
    }
}

function resizeBody() {
    document.body.style.width = window.innerWidth + "px";
    document.body.style.height = window.innerHeight + "px";
}
window.addEventListener("resize",() => {
    resizeBody();
})
window.addEventListener("beforeunload",(e) => {
    if(loading_container.style.display) { // 刷新提示
        e.preventDefault();
        e.returnValue= "";
    }
})
window.addEventListener("load",() => {
    audioElement.muted = true;
    videoElement.muted = true;

    resizeBody();
    initButs();
    showMediaContainer(false);
    dragElement(hoverBut,() => {
        showControlBox(true);
        infoIndex = (infoIndex + 1) % 2;
    });
})