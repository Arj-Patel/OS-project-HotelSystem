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
        if (sscanf(line, "%d.%49[^0-9] %f", &serialNumber, itemName, &price) == 3)
        {
            menu[*itemCount].serial_number = serialNumber;
            strncpy(menu[*itemCount].name, itemName, sizeof(menu[*itemCount].name) - 1);
            menu[*itemCount].price = price;
            (*itemCount)++;
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
        printf("%d. %s - %.2f INR\n", menu[i].serial_number, menu[i].name, menu[i].price);
    }
}

int main()
{
    int table_id;
    printf("Enter Table Number: ");
    scanf("%d", &table_id);

    MenuItem menu[100];                // Menu array
    int itemCount = 0;                 // Number of items in the menu
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
    // printf("%d", table_key);

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

    (*shared_order)[0][5] = 0;

    while (1)
    {

        int num_customers;
        printf("Enter Number of Customers at Table (maximum no. of customers can be 5): ");
        scanf("%d", &num_customers);

        // printf("%d\n", num_customers);
        // (*shared_order)[0][0] = num_customers;
        (*shared_order)[0][4] = 0;

        if (num_customers == -1)
            break;

        int parentToChildPipes[num_customers][2];
        int childToParentPipes[num_customers][2];
        pid_t pids[num_customers];

        // Create pipes
        for (int i = 0; i < num_customers; i++)
        {
            if (pipe(parentToChildPipes[i]) == -1 || pipe(childToParentPipes[i]) == -1)
            {
                perror("pipe creation failed");
                exit(1);
            }
        }

        displayMenu(menu, itemCount);
        // Fork child processes
        for (int i = 0; i < num_customers; i++)
        {
            pids[i] = fork();
            if (pids[i] < 0)
            {
                perror("fork failed");
                exit(1);
            }
            else if (pids[i] == 0)
            { // Child process
                close(parentToChildPipes[i][1]);
                close(childToParentPipes[i][0]);

                int flag;
                do
                {
                    // Read flag from parent
                    read(parentToChildPipes[i][0], &flag, sizeof(flag));

                    if (flag == 1)
                    { // Take order
                        int order[256] = {0}, idx = 0, srNo;
                        printf("Customer %d, enter your order (serial numbers, -1 to finish):\n", i + 1);
                        do
                        {
                            scanf("%d", &srNo);
                            if (srNo != -1)
                            {
                                order[idx++] = srNo;
                            }
                        } while (srNo != -1);

                        // Send order to parent
                        int ack = 1; // Acknowledgment signal
                        write(childToParentPipes[i][1], &ack, sizeof(ack));
                        write(childToParentPipes[i][1], order, idx * sizeof(int));
                    }
                } while (flag != 2); // 2 means exit

                // Cleanup and exit child process
                close(parentToChildPipes[i][0]);
                close(childToParentPipes[i][1]);
                exit(0);
            }
            else
            {
                close(parentToChildPipes[i][0]);
                close(childToParentPipes[i][1]);
            }
        }

        (*shared_order)[0][0] = num_customers;
        int allOrdersValid = 0;
        while (!allOrdersValid)
        {
            allOrdersValid = 1;

            for (int i = 0; i < num_customers; i++)
            {
                int flag = 1; // Signal to take order
                write(parentToChildPipes[i][1], &flag, sizeof(flag));
                read(childToParentPipes[i][0], &flag, sizeof(flag));
            }

            printf("here2\n");

            printf("%d\n", num_customers);

            for (int i = 0; i < num_customers; i++)
            {
                printf("%d ", i);
                int order[256];
                int bytesRead = read(childToParentPipes[i][0], order, sizeof(order));
                int numItems = bytesRead / sizeof(int);
                (*shared_order)[i + 1][0] = numItems;
                // printf("%d\n", i);
                for (int j = 1; j <= numItems; j++)
                {
                    (*shared_order)[i + 1][j] = order[j - 1];
                }
            }

            printf("here1\n");

            // for (int i = 1; i <= num_customers; i++)
            // {
            //     // printf("%d\n", i);
            //     for (int j = 1; j <= (*shared_order)[i][0]; j++)
            //     {
            //         printf("%d ", (*shared_order)[i][j]);
            //     }
            //     printf("\n");
            // }

            // (*shared_order)[0][4] = 1;
            // (*shared_order)[0][1] = -1;
            // printf("%d\n",(*shared_order)[0][0]);
            while (1)
            // while ((*shared_order)[0][1] == -1)
            {
                printf("sleep");
                // printf("here");
                sleep(1);
            }

            allOrdersValid = (*shared_order)[0][1];

            if (!allOrdersValid)
            {
                printf("invalid orders, taking all orders again\n");
            }
        }

        (*shared_order)[0][2] = 0;

        while ((*shared_order)[0][2] == 0)
            sleep(1);

        int totalBill = (*shared_order)[0][3];

        printf("The total bill amount is %d INR. ", totalBill);
        printf("\n");

        for (int i = 0; i < num_customers; i++)
        {
            int flag = 2; // Signal to exit
            write(parentToChildPipes[i][1], &flag, sizeof(flag));
            waitpid(pids[i], NULL, 0); // Wait for child process to terminate
        }

        for (int i = 0; i < num_customers; i++)
        {
            close(parentToChildPipes[i][1]);
            close(childToParentPipes[i][0]);
        }
    }

    (*shared_order)[0][5] = 1;

    if (shmdt(shared_order) == -1)
    {
        perror("shmdt");
        exit(1);
    }

    if (shmctl(table_id, IPC_RMID, NULL) == -1)
    {
        perror("shmctl");
        exit(1);
    }

    printf("Table process terminated.\n");
    return 0;
}
