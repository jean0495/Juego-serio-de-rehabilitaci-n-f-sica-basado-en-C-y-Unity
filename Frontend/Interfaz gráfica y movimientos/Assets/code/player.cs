using UnityEngine;
using System.IO.Ports;
using System.Text.RegularExpressions;

public class player : MonoBehaviour
{
    private manager manager;
    [HideInInspector] public Animator animimator;
    private Vector3 targetPosition;
    private bool isMoving;
    private bool isrolling = false;
    [HideInInspector] public bool isjumping = false;
    public float lanesdistancia = 1f;

    // Serial config - UN SOLO PUERTO para ambos sensores
    private SerialPort serialPort;
    public string portName = "COM15"; // El mismo puerto para EMG y Giroscopio
    public int baudRate = 9600;
    
    // EMG threshold para salto
    public int emgThreshold = 700;
    
    // Buffer para datos seriales
    private string serialBuffer = "";

    void Start()
    {
        manager = GetComponent<manager>();
        animimator = GetComponent<Animator>();
        targetPosition = transform.position;
        isMoving = false;

        // Inicializa UN SOLO puerto serial para ambos sensores
        try
        {
            serialPort = new SerialPort(portName, baudRate);
            serialPort.Open();
            serialPort.ReadTimeout = 100;
            Debug.Log("Puerto serial abierto exitosamente en " + portName);
        }
        catch (System.Exception e)
        {
            Debug.LogError("Error abriendo el puerto serial: " + e.Message);
        }
    }

    void Update()
    {
        SelectTargetPositionKeyboard(); // Mantener control por teclado
        MoveToTargetPosition();
        checkroll();
        checkjump();
        ReadSerialData(); // Leer TODOS los datos seriales (EMG y Giroscopio)
    }

    // Control por teclado (se mantiene como respaldo)
    private void SelectTargetPositionKeyboard()
    {
        if (isBusy()) return;

        float horizontal = manager.horizontal.ReadValue<float>();
        float x = transform.position.x;

        if (horizontal == 1 && x <= 0)
        {
            targetPosition = transform.position + Vector3.right * lanesdistancia;
            isMoving = true;
        }
        else if (horizontal == -1 && x >= 0)
        {
            targetPosition = transform.position + Vector3.left * lanesdistancia;
            isMoving = true;
        }
    }

    private void MoveToTargetPosition()
    {
        if (!isMoving) return;

        transform.position = Vector3.Lerp(transform.position, targetPosition, 0.1f);
        float distancia = (targetPosition - transform.position).magnitude;
        if (distancia < 0.1f)
        {
            transform.position = targetPosition;
            isMoving = false;
        }
    }

    private void checkroll()
    {
        if (isBusy()) return;

        if (manager.role.IsPressed())
        {
            animimator.SetTrigger("roll");
            isrolling = true;
        }
    }

    public void Endroll()
    {
        isrolling = false;
    }

    private void checkjump()
    {
        if (isBusy()) return;

        if (manager.jump.IsPressed())
        {
            //animimator.SetTrigger("jump");
            isjumping = true;
        }
    }

    public void Endjump()
    {
        isjumping = false;
    }

    public bool isBusy()
    {
        return isMoving || isrolling || isjumping;
    }

    private void OnTriggerEnter(Collider other)
    {
        if (other.tag == "smallobstacle")
        {
            animimator.SetTrigger("stumble");
        }
    }











    // Leer todos los datos seriales (EMG y Giroscopio en el mismo puerto)

//FUNCIÓN PARA LEER EL PUERTO SERIAL   
    private void ReadSerialData()
    {
        if (serialPort != null && serialPort.IsOpen)
        {
            try
            {
                string newData = serialPort.ReadExisting();
                
                if (!string.IsNullOrEmpty(newData))
                {
                    serialBuffer += newData;
                    
                    string[] lines = serialBuffer.Split('\n');
                    
                    for (int i = 0; i < lines.Length - 1; i++)
                    {
                        ProcessSerialLine(lines[i].Trim());
                    }
                    
                    serialBuffer = lines[lines.Length - 1];
                    
                    if (serialBuffer.Length > 1000)
                    {
                        serialBuffer = "";
                    }
                }
            }
            catch (System.TimeoutException)
            {
                // Timeout normal, no hacer nada
            }
            catch (System.Exception e)
            {
                Debug.LogWarning("Error leyendo puerto serial: " + e.Message);
            }
        }
    }
    
    // Procesar línea serial y determinar si es EMG o Giroscopio por el formato
    private void ProcessSerialLine(string line)
    {
        if (string.IsNullOrEmpty(line)) return;
        
        Debug.Log($"Datos recibidos: {line}");
        
        // PROCESAR DATOS EMG - Buscar patrón RAW:valor
        Regex emgRegex = new Regex(@"EMG:(\d+)");
        Match emgMatch = emgRegex.Match(line);
        
        if (emgMatch.Success)
        {
            ProcessEMGData(line, emgMatch);
            return; // Si es EMG, no procesar como giroscopio
        }
        
        // PROCESAR DATOS GIROSCOPIO - Si no tiene patrón RAW:, es giroscopio
        ProcessGyroData(line);
    }

//*************************************************************************************************************************************************************************








//FUNCIÓN PARA EL EMG****************************************************************************************************************************************************    

    private void ProcessEMGData(string line, Match match)
    {
        if (int.TryParse(match.Groups[1].Value, out int rawValue))
        {
            Debug.Log($"Valor EMG RAW detectado: {rawValue}");
            
            if (rawValue > emgThreshold && !isBusy())
            {
                Debug.Log("¡Salto activado por EMG!");
                animimator.SetTrigger("jump");
                isjumping = true;
            }
        }
    }


//**************************************************************************************************************************************************************************









//FUNCIÓN DEL GIROSCOPIO********************************************************************************************************************************************


    private void ProcessGyroData(string line)
    {
        // SIEMPRE imprimir los datos del giroscopio con información detallada
        Debug.Log($"[GIROSCOPIO] Dato recibido: '{line}'");
        Debug.Log($"[GIROSCOPIO] Longitud: {line.Length} caracteres");
        Debug.Log($"[GIROSCOPIO] Bytes: [{string.Join(", ", System.Text.Encoding.UTF8.GetBytes(line))}]");
        
        if (isBusy()) 
        {
            Debug.Log("[GIROSCOPIO] Jugador ocupado, ignorando comando");
            return;
        }
        
        float x = transform.position.x;
        
        // Procesar comandos del giroscopio
        string upperLine = line.ToUpper().Trim();
        Debug.Log($"[GIROSCOPIO] Procesando como: '{upperLine}'");
        
        if (upperLine.Contains("CENTRO") || upperLine.Contains("CENTER") || upperLine == "C")
        {
            Debug.Log("[GIROSCOPIO] Comando CENTRO detectado");
            if (x != 0) // Solo moverse al centro si no está ya en el centro
            {
                targetPosition = new Vector3(0, transform.position.y, transform.position.z);
                isMoving = true;
                Debug.Log("[GIROSCOPIO] ✓ Movimiento al CENTRO ACTIVADO");
            }
            else
            {
                Debug.Log("[GIROSCOPIO] ✓ Ya está en el centro, posición mantenida");
            }
        }
        else if (upperLine.Contains("IZQUIERDA") || upperLine.Contains("LEFT") || upperLine == "L")
        {
            Debug.Log("[GIROSCOPIO] Comando IZQUIERDA detectado");
            if (x >= 0) // Solo moverse a la izquierda si no está ya en el extremo izquierdo
            {
                targetPosition = transform.position + Vector3.left * lanesdistancia;
                isMoving = true;
                Debug.Log("[GIROSCOPIO] ✓ Movimiento a la izquierda ACTIVADO");
            }
            else
            {
                Debug.Log("[GIROSCOPIO] ✗ Ya está en el extremo izquierdo, movimiento ignorado");
            }
        }
        else if (upperLine.Contains("DERECHA") || upperLine.Contains("RIGHT") || upperLine == "R")
        {
            Debug.Log("[GIROSCOPIO] Comando DERECHA detectado");
            if (x <= 0) // Solo moverse a la derecha si no está ya en el extremo derecho
            {
                targetPosition = transform.position + Vector3.right * lanesdistancia;
                isMoving = true;
                Debug.Log("[GIROSCOPIO] ✓ Movimiento a la derecha ACTIVADO");
            }
            else
            {
                Debug.Log("[GIROSCOPIO] ✗ Ya está en el extremo derecho, movimiento ignorado");
            }
        }
        
        // Mantener compatibilidad con el sistema anterior para salto
        else if (line == "1")
        {
            Debug.Log("[GIROSCOPIO] Comando de salto '1' detectado");
            if (!isBusy())
            {
                animimator.SetTrigger("jump");
                isjumping = true;
                Debug.Log("[GIROSCOPIO] ✓ Salto activado");
            }
            else
            {
                Debug.Log("[GIROSCOPIO] ✗ Jugador ocupado, salto ignorado");
            }
        }
        else
        {
            Debug.Log($"[GIROSCOPIO] ⚠️  Comando no reconocido: '{line}'");
        }
    }

    void OnDestroy()
    {
        if (serialPort != null && serialPort.IsOpen)
        {
            serialPort.Close();
            Debug.Log("Puerto serial cerrado");
        }
    }
}