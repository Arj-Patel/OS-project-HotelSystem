#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>

typedef struct
{
    int serial_number;
    char name[50];
    float price;
} MenuItem;

int readMenu(const char *filename, MenuItem menu[], int *itemCount)
{
    FILE *file;
    char line[100];
    int serialNumber;
    char itemName[50];
    float price;

    file = fopen(filename, "r");
    if (file == NULL)
    {
        perror("Error opening file");
        return 0; // Return 0 to indicate failure
    }

    while (fgets(line, sizeof(line), file))
    {
        serialNumber = 0;
        memset(itemName, 0, sizeof(itemName));
        price = 0.0f;

        char *token = strtok(line, ".");
        if (token != NULL)
        {
            serialNumber = atoi(token);
            token = strtok(NULL, "INR");
            if (token != NULL)
            {
                sscanf(token, "%49[^0-9] %f", itemName, &price);
                menu[*itemCount].serial_number = serialNumber;
                strncpy(menu[*itemCount].name, itemName, sizeof(menu[*itemCount].name) - 1);
                menu[*itemCount].price = price;
                (*itemCount)++;
            }
        }
    }

    fclose(file);
    return 1; // Return 1 to indicate success
}

void displayMenu(const MenuItem menu[], int itemCount)
{
    printf("Menu:\n");
    for (int i = 0; i < itemCount; i++)
    {
        printf("%d. %s %.2f INR\n", menu[i].serial_number, menu[i].name, menu[i].price);
    }
}

int isValidOrder(const int order[], int size, const MenuItem menu[], int itemCount)
{

    for (int i = 0; i < size; i++)
    {
        int found = 0;
        for (int j = 0; j < itemCount; j++)
        {
            if (order[i] == menu[j].serial_number)
            {
                found = 1;
                break; // Break out of inner loop if a match is found
            }
        }
        if (!found)
        {
            return 0; // Return false if no matching serial number is found for the current order item
        }
    }
    return 1; // Return true if all order items match a serial number
}

int main()
{

    int table_id;
    printf("Enter Table Number: ");
    scanf("%d", &table_id);

    MenuItem menu[100]; // Menu array
    int itemCount = 0;  // Number of items in the menu

    const char *filename = "menu.txt"; // File containing the menu

    // Read menu items from file
    if (!readMenu(filename, menu, &itemCount))
    {
        fprintf(stderr, "Failed to read menu from %s\n", filename);
        return -1;
    }

    key_t table_key = ftok(".", table_id);
    if (table_key == -1)
    {
        perror("ftok1");
        exit(1);
    }

    int table_shm_id = shmget(table_key, sizeof(int) * 6 * 256, IPC_CREAT | 0666);
    if (table_shm_id == -1)
    {
        perror("shmget");
        exit(1);
    }

    int(*shared_order)[6][256];

    shared_order = (int(*)[6][256])shmat(table_shm_id, NULL, 0);
    if (shared_order == (void *)-1)
    {
        perror("shmat");
        exit(1);
    }

    printf("Enter the number of customers: ");
    int num_customers;
    scanf("%d", &num_customers);

    (*shared_order)[0][0] = num_customers; // the (0,0) of the shared memory will have numCustomers, you will need to access numcustomers in waiter table
                                        // the first row of the shared memory can be used as a flag register to communicate between waiter and table
                                        // order of the ith customer will be stored in (i+1)th row

    for (int i = 0; i < num_customers; i++)
    {
        int order[256];
        int srNo;
        scanf("%d", &srNo);
        int idx;
        while (srNo != -1)
        {
            order[idx++] = srNo;
            scanf("%d", &srNo);
        }
        (*shared_order)[i+1][0] = num_customers; // the first index of the (i+1)th row in shared memory will indicate the number of items ith customer has order
        for(int j = 1; j <= idx; j++){
            (*shared_order)[i+1][j] = order[j-1];
        }

    }

    if (shmdt(shared_order) == -1)
    {
        perror("shmdt");
        exit(1);
    }

    if (shmctl(table_shm_id, IPC_RMID, NULL) == -1)
    {
        perror("shmctl");
        exit(1);
    }

    printf("Table process terminated.\n");
    return 0;
}
