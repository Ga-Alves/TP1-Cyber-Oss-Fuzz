#include <stdio.h>
#include <stdlib.h>
#include "cJSON.h"  // Include the cJSON header

int main() {
    // Create a JSON object
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        fprintf(stderr, "Failed to create JSON object.\n");
        return 1;
    }

    // Add key-value pairs to the JSON object
    cJSON_AddStringToObject(root, "name", "John Doe");
    cJSON_AddNumberToObject(root, "age", 30);
    cJSON_AddBoolToObject(root, "is_student", 0);  // 0 = false, 1 = true

    // Create a nested JSON object
    cJSON *address = cJSON_CreateObject();
    cJSON_AddStringToObject(address, "street", "123 Main St");
    cJSON_AddStringToObject(address, "city", "Anytown");
    cJSON_AddStringToObject(address, "state", "CA");
    cJSON_AddItemToObject(root, "address", address);

    // Create a JSON array
    cJSON *hobbies = cJSON_CreateArray();
    cJSON_AddItemToArray(hobbies, cJSON_CreateString("reading"));
    cJSON_AddItemToArray(hobbies, cJSON_CreateString("hiking"));
    cJSON_AddItemToArray(hobbies, cJSON_CreateString("coding"));
    cJSON_AddItemToObject(root, "hobbies", hobbies);

    // Print the JSON object as a string
    char *json_string = cJSON_Print(root);
    if (json_string == NULL) {
        fprintf(stderr, "Failed to print JSON.\n");
    } else {
        printf("Created JSON:\n%s\n", json_string);
        free(json_string);  // Free the allocated string
    }

    // Parse a JSON string
    const char *json_data = "{\"name\":\"Jane Doe\",\"age\":25,\"is_student\":true}";
    cJSON *parsed_json = cJSON_Parse(json_data);
    if (parsed_json == NULL) {
        fprintf(stderr, "Failed to parse JSON.\n");
    } else {
        // Extract values from the parsed JSON
        cJSON *name = cJSON_GetObjectItemCaseSensitive(parsed_json, "name");
        cJSON *age = cJSON_GetObjectItemCaseSensitive(parsed_json, "age");
        cJSON *is_student = cJSON_GetObjectItemCaseSensitive(parsed_json, "is_student");

        if (cJSON_IsString(name) && cJSON_IsNumber(age) && cJSON_IsBool(is_student)) {
            printf("\nParsed JSON:\n");
            printf("Name: %s\n", name->valuestring);
            printf("Age: %d\n", age->valueint);
            printf("Is Student: %s\n", is_student->valueint ? "true" : "false");
        }
        cJSON_Delete(parsed_json);  // Free the parsed JSON object
    }

    // Clean up
    cJSON_Delete(root);
    return 0;
}