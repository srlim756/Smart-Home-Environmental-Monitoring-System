const SENSOR_NAMES = ['temp', 'humi', 'light', 'smoke'];
let alarmTimeout = null;

function toTime(ts) {
    if (!ts) return '--';
    const d = new Date(ts * 1000);
    return d.toLocaleTimeString();
}

function fetchCurrent() {
    fetch('/api/current')
        .then(r => r.json())
        .then(data => {
            if (data.stale) {
                document.getElementById('status-text').textContent = '传感器离线';
                document.getElementById('status-dot').className = 'dot inactive';
                return;
            }
            SENSOR_NAMES.forEach(name => {
                const el = document.getElementById(name);
                if (el && data[name] !== undefined) {
                    el.textContent = data[name].toFixed(1);
                }
            });
            document.getElementById('status-text').textContent = '运行中 ' + toTime(data.temp_ts);
            document.getElementById('status-dot').className = 'dot active';
        })
        .catch(() => {
            document.getElementById('status-text').textContent = '连接中...';
            document.getElementById('status-dot').className = 'dot inactive';
        });
}

function showAlarm(data) {
    const banner = document.getElementById('alarm-banner');
    banner.textContent = data.type + ' 告警! 数值: ' + data.value.toFixed(1);
    banner.className = 'alarm-banner';

    const typeIndex = SENSOR_NAMES.indexOf(data.type.toLowerCase());
    const cards = document.querySelectorAll('.card');
    if (cards[typeIndex]) {
        cards[typeIndex].classList.add('alarm');
    }

    clearTimeout(alarmTimeout);
    alarmTimeout = setTimeout(() => {
        banner.className = 'alarm-banner hidden';
        document.querySelectorAll('.card').forEach(c => c.classList.remove('alarm'));
    }, 5000);

    const countEl = document.getElementById('alarm-count');
    countEl.textContent = parseInt(countEl.textContent) + 1;
}

function connectSSE() {
    const evtSource = new EventSource('/api/events');
    evtSource.onmessage = function(e) {
        try {
            const data = JSON.parse(e.data);
            showAlarm(data);
        } catch (err) {
            console.log('SSE:', e.data);
        }
    };
    evtSource.onerror = function() {
        console.log('SSE disconnected, reconnecting...');
        setTimeout(connectSSE, 2000);
    };
}

fetchCurrent();
setInterval(fetchCurrent, 2000);
connectSSE();
