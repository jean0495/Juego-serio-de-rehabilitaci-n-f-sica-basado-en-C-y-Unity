using UnityEngine;
using System.Collections.Generic;
using System.Collections;
using UnityEngine.InputSystem;

public class manager : MonoBehaviour
{
    private Control controli;
    public InputAction horizontal;
    public InputAction role;
    public InputAction jump;

    private void Awake()
    {
        controli = new Control();
        horizontal = controli.ingame.horizontal;
        horizontal.Enable();

        role = controli.ingame.role;
        role.Enable();
        jump = controli.ingame.jump;
        jump.Enable();

    }
    public void OnDisable(){
        horizontal.Disable();
        role.Disable();
        jump.Disable();

    } 


}
