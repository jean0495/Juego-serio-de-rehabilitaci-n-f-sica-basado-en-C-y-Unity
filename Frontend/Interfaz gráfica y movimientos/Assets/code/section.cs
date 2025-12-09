using UnityEngine;
using System.Collections.Generic; // Necesario para usar List<>

public class section : MonoBehaviour 
{
    private int sectioncount = 0;
    public List<GameObject> obstaculo;
    private static int lastRandomIndex = -1;
    
    public float Speed;
    
   


    void Start() // Start con mayúscula - era "start"
    {
        sectioncount = GameObject.FindGameObjectsWithTag("section").Length;
        obstaculo = new List<GameObject>();
        foreach (Transform child in transform)
        {
             if (child.tag == "obstaculo") // "child" no "chile"
             {
                obstaculo.Add(child.gameObject); // Punto y coma, no coma
             }
        }
         
        EnableRandomObstacle();
    }
    
    public void EnableRandomObstacle()
    {
        foreach (GameObject obstaculo in obstaculo)
        {
            obstaculo.SetActive(false);
        }
        
        int randomIndex = lastRandomIndex;
        while(randomIndex == lastRandomIndex){
           randomIndex = Random.Range(0, obstaculo.Count); // Declaración corregida
        }

      
        lastRandomIndex = randomIndex;
        obstaculo[randomIndex].SetActive(true);
    }

    void Update(){
        transform.Translate(Vector3.back* Speed *Time.deltaTime);
        if (transform.position.z <= -20){
            transform.Translate(Vector3.forward*20*sectioncount);
            EnableRandomObstacle();
        }
    }

}