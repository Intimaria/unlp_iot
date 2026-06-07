document.addEventListener('DOMContentLoaded', () => {
    const tempVal = document.getElementById('temp-val');
    const tempProgress = document.getElementById('temp-progress');
    const humVal = document.getElementById('hum-val');
    const humProgress = document.getElementById('hum-progress');
    const co2Val = document.getElementById('co2-val');
    const coVal = document.getElementById('co-val');
    const alcoholVal = document.getElementById('alcohol-val');
    const nh4Val = document.getElementById('nh4-val');
    const toluenoVal = document.getElementById('tolueno-val');
    const lightVal = document.getElementById('light-val');
    const lightStatus = document.getElementById('light-status');

    const ledSwitch = document.getElementById('led-switch');
    const ledStatusText = document.getElementById('led-status-text');
    const brokerIpDisplay = document.getElementById('broker-ip');
    const lastUpdateDisplay = document.getElementById('last-update');
    const mqttStatusBadge = document.getElementById('mqtt-status');
    const mqttStatusText = mqttStatusBadge.querySelector('.status-text');

    let mqttClient = null;

    // 1. Obtener la IP del broker desde el ESP32
    fetch('/api/config')
        .then(response => {
            if (!response.ok) {
                throw new Error('No se pudo obtener la configuración del ESP32');
            }
            return response.json();
        })
        .then(config => {
            const brokerIp = config.broker_ip || window.location.hostname;
            brokerIpDisplay.textContent = brokerIp;
            connectMQTT(brokerIp);
        })
        .catch(error => {
            console.error('Error al obtener la configuración:', error);
            // Fallback: usar la misma IP de donde se cargó la página
            const fallbackIp = window.location.hostname || '127.0.0.1';
            brokerIpDisplay.textContent = fallbackIp + ' (Fallback)';
            connectMQTT(fallbackIp);
        });

    // 2. Conectar al Broker MQTT a través de WebSockets
    function connectMQTT(brokerIp) {
        console.log(`Conectando al Broker MQTT en ws://${brokerIp}:9001...`);

        try {
            // MQTT.js sobre WebSockets
            mqttClient = mqtt.connect(`ws://${brokerIp}:9001`, {
                clientId: 'web-client-' + Math.random().toString(16).substring(2, 8),
                clean: true,
                connectTimeout: 5000,
                reconnectPeriod: 2000
            });

            mqttClient.on('connect', () => {
                console.log('¡Conectado exitosamente al broker MQTT!');
                mqttStatusBadge.className = 'status-badge connected';
                mqttStatusText.textContent = 'Conectado';

                // Suscribirse a los topics de telemetría y estado de actuadores
                mqttClient.subscribe('sensor/ambiente');
                mqttClient.subscribe('sensor/led/state');
            });

            mqttClient.on('close', () => {
                console.warn('Conexión con el broker MQTT perdida.');
                mqttStatusBadge.className = 'status-badge disconnected';
                mqttStatusText.textContent = 'Desconectado';
            });

            mqttClient.on('error', (err) => {
                console.error('Error del cliente MQTT:', err);
                mqttStatusBadge.className = 'status-badge disconnected';
                mqttStatusText.textContent = 'Error';
            });

            mqttClient.on('message', (topic, message) => {
                const payloadStr = message.toString();
                const now = new Date();
                lastUpdateDisplay.textContent = now.toLocaleTimeString();

                if (topic === 'sensor/ambiente') {
                    handleTelemetry(payloadStr);
                } else if (topic === 'sensor/led/state') {
                    handleLedState(payloadStr);
                }
            });
        } catch (e) {
            console.error('Error al iniciar la librería MQTT:', e);
        }
    }

    // 3. Manejar los datos de los sensores
    function handleTelemetry(payloadStr) {
        try {
            const data = JSON.parse(payloadStr);
            console.log('Datos recibidos:', data);

            // Temperatura
            if (data.temp !== undefined) {
                const t = parseFloat(data.temp);
                tempVal.textContent = t.toFixed(1);
                // Mapear temperatura a porcentaje (rango -10 a 50)
                let tempPercent = ((t + 10) / 60) * 100;
                tempPercent = Math.max(0, Math.min(100, tempPercent));
                tempProgress.style.width = `${tempPercent}%`;
            }

            // Humedad
            if (data.hum !== undefined) {
                const h = parseFloat(data.hum);
                humVal.textContent = h.toFixed(1);
                humProgress.style.width = `${Math.max(0, Math.min(100, h))}%`;
            }

            // Calidad del Aire (Gases MQ135)
            if (data.co2 !== undefined) {
                co2Val.textContent = parseFloat(data.co2).toFixed(1) + ' ppm';
            }
            if (data.co !== undefined) {
                coVal.textContent = parseFloat(data.co).toFixed(2) + ' ppm';
            }
            if (data.alcohol !== undefined) {
                alcoholVal.textContent = parseFloat(data.alcohol).toFixed(2) + ' ppm';
            }
            if (data.nh4 !== undefined) {
                nh4Val.textContent = parseFloat(data.nh4).toFixed(2) + ' ppm';
            }
            if (data.tolueno !== undefined) {
                toluenoVal.textContent = parseFloat(data.tolueno).toFixed(2) + ' ppm';
            }

            // Luminosidad (LDR)
            // LDR con divisor a GND: Voltaje sube al haber más luz (ADC más alto)
            if (data.luz !== undefined) {
                const l = parseInt(data.luz);
                lightVal.textContent = l;

                if (l > 3000) {
                    lightStatus.textContent = 'Soleado / Día';
                    lightStatus.className = 'label-normal';
                } else if (l > 1000 && l <= 3000) {
                    lightStatus.textContent = 'Luz Artificial / Nublado';
                    lightStatus.className = 'label-warning';
                } else {
                    lightStatus.textContent = 'Oscuro / Noche';
                    lightStatus.className = 'label-normal';
                    // Cambiar estilo de texto a apagado si está oscuro
                    lightStatus.style.color = '#8b5cf6';
                }
            }

        } catch (e) {
            console.error('Error al parsear el JSON de telemetría:', e);
        }
    }

    // 4. Manejar el estado del LED (desde el broker)
    function handleLedState(payloadStr) {
        const state = payloadStr.trim();
        console.log('Estado de LED recibido:', state);

        if (state === '1') {
            ledSwitch.checked = true;
            ledStatusText.textContent = 'Encendido';
            ledStatusText.className = 'led-on';
        } else {
            ledSwitch.checked = false;
            ledStatusText.textContent = 'Apagado';
            ledStatusText.className = 'led-off';
        }
    }

    // 5. Publicar comandos de control al interactuar con el Switch
    ledSwitch.addEventListener('change', () => {
        if (!mqttClient || !mqttClient.connected) {
            alert('No estás conectado al broker MQTT. No se puede enviar el comando.');
            // Revertir switch
            ledSwitch.checked = !ledSwitch.checked;
            return;
        }

        const cmd = ledSwitch.checked ? '1' : '0';
        console.log('Publicando comando de control de LED:', cmd);
        mqttClient.publish('sensor/led/control', cmd, { qos: 1, retain: true });
    });
});
