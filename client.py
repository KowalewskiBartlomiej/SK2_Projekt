import tkinter as tk
import socket
import argparse

BUFFSIZE = 302
TOPICLENGTH = 99
TOPICSNUMBER = 2
MSG_LENGTH = 200


#Wczytywanie adresu serwera i nr portu jako argumentów przy uruchamianiu 
parser = argparse.ArgumentParser()
parser.add_argument("server_address")
parser.add_argument("port")
args = parser.parse_args()  


#Funkcje dodające poszczególne elementy interfejsu
def add_window(size = '300x200', title = "Client App"):
    window = tk.Tk()
    window.geometry(size)
    window.title(title)
    return window

def add_label(anchor, txt = ""):
    label = tk.Label(master = anchor, text = txt)
    label.pack()
    return label

def add_entry(anchor):
    entry = tk.Entry(master = anchor)
    entry.pack()
    return entry

def add_button(anchor, txt = ""):
    button = tk.Button(master = anchor, text = txt)
    button.pack(fill="both")
    return button

def buttons(root):
    for i in "Dodaj temat", "Wyślij wiadomość", "Zasubskrybuj temat", "Anuluj subskrybcję", "Wyświetl tematy", "Odbierz wiadomości", "Wyjście":
        b = tk.Button(master=root, text=i)
        b.pack(side="top", fill="both")
        yield b


#Funkcja łącząca z serwerem i w przypadku 
#udanego połaczenia tworząca główne okno aplikacji
def connect(server_addr, port):
    feedback_window = add_window()
    button = add_button(feedback_window, "OK")
    button.configure(command = feedback_window.destroy)

    try:
        connection_socket_description = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    except:
        add_label(feedback_window, "Nie udało się stworzyć socketu")

    connection_socket_description.connect((server_addr, port))
    feedback = str(connection_socket_description.recv(BUFFSIZE).decode())
    
    if feedback[0] == "r":
        add_label(feedback_window, feedback[1:])
        button.configure(command = lambda:[feedback_window.destroy(), exit()])
        feedback_window.mainloop()
        return None, None
    else:
        add_label(feedback_window, feedback)
        root = add_window('600x400', "Publish/subscribe Client")
        add_label(root, "\nWitamy w aplikacji publish/subscribe!\n\n Wybierz jedną z poniższych opcji: \n")
        return root, connection_socket_description


#Funkcja kończąca działanie klienta
def close_app(root, sockfd):
    sockfd.send('e'.encode())
    sockfd.close()
    root.destroy()


#Funkcja odpowiadająca za wyświetlanie wiadomości
def display_msg(sockfd):
    sockfd.send('w'.encode())
    message_num = str(sockfd.recv(TOPICSNUMBER).decode())
    messages_window = add_window('600x200', "Wiadomości")
    add_label(messages_window, "Wiadomości: \n")
    for i in range(int(message_num[0])):
        message = str(sockfd.recv(BUFFSIZE).decode())
        topic_length = int(message[1:3])
        add_label(messages_window, message[3:3+topic_length] + ":\t" + message[3+topic_length:])
    exit_button = add_button(messages_window, "OK")
    exit_button.configure(command=messages_window.destroy)


#Funkcja odpowiadająca za wyświetlanie tematów
def display_topics(sockfd):
    sockfd.send('t'.encode())
    topics_num = str(sockfd.recv(TOPICSNUMBER).decode())
    topics_window = add_window('500x200')
    add_label(topics_window, "Aktualna lista tematów:")
    add_label(topics_window, "* przy temacie - zasubskrybowany przez klienta \n")
    for i in range(int(topics_num[0])):
        add_label(topics_window, str(sockfd.recv(TOPICLENGTH).decode()))
    exit_button = add_button(topics_window, "OK")
    exit_button.configure(command=topics_window.destroy)


#Funkcja odpowiadająca za wykonanie konkretnej akcji
def action(akcja, sockfd, entry, view, msg_entry = None):
    feedback = ""
    msg = akcja
    topic = str(entry.get())
    if len(topic) == 0:
        err_window = add_window(title="Błąd")
        add_label(err_window, "Nie podano tematu")
        exit_button = add_button(err_window, "OK")
        exit_button.configure(command=err_window.destroy)
    elif len(topic) > TOPICLENGTH:
        err_window = add_window(title="Błąd")
        add_label(err_window, "Podano za długi temat")
        exit_button = add_button(err_window, "OK")
        exit_button.configure(command=err_window.destroy)
    else:
        if len(topic) < 10:
            msg+='0'
        msg+=str(len(topic))
        msg+=topic

        #Wysyłanie wiadomości
        if akcja == "s":
            message = str(msg_entry.get())
            if len(message) == 0:
                err_window = add_window(title="Błąd")
                add_label(err_window, "Nie podano treści wiadomości")
                exit_button = add_button(err_window, "OK")
                exit_button.configure(command=err_window.destroy)
            elif len(message) > MSG_LENGTH:
                err_window = add_window(title="Błąd")
                add_label(err_window, "Wiadomość przekracza liczbę znaków")
                exit_button = add_button(err_window, "OK")
                exit_button.configure(command=err_window.destroy)
            else:
                msg+=message
                sockfd.send(msg.encode())
                feedback = str(sockfd.recv(BUFFSIZE).decode())
                feedback_window = add_window(title = 'Server Feedback')
                feedback_label = add_label(feedback_window)
                exit_button = add_button(feedback_window, "OK")
                exit_button.configure(command=feedback_window.destroy)
        
        #Subskrybcja, dodawanie tematu, anulowanie subskrybcji
        else:
            print(msg)
            sockfd.send(msg.encode())
            feedback = str(sockfd.recv(BUFFSIZE).decode())
            feedback_window = add_window(title = 'Server Feedback')
            feedback_label = add_label(feedback_window)
            exit_button = add_button(feedback_window, "OK")
            exit_button.configure(command=feedback_window.destroy)

        if feedback:
            if feedback[0] == "r":
                feedback_label.configure(text = feedback[1:])
            else:
                feedback_label.configure(text = feedback)
                view.destroy()


#Funkcja odpowiadająca za stworzenie okna do wprowadzania tematu
def insert_view():
    inserting_window = add_window()
    add_label(inserting_window, "Wprowadź nazwę tematu: ")
    entry = add_entry(inserting_window)
    apply_button = add_button(inserting_window, "Zatwierdź")
    cancel_button = add_button(inserting_window, "Anuluj")
    cancel_button.configure(command=inserting_window.destroy)
    return inserting_window, entry, apply_button


#Funkcje, które przypisane są do poszczególnych przycisków 
def insert_topic(sockfd):
    window, entry, button = insert_view()
    button.configure(command=lambda:action("a", sockfd, entry, window))

def subscribe_topic(sockfd):
    window, entry, button = insert_view()
    button.configure(command=lambda:action("f", sockfd, entry, window))

def unsubscribe_topic(sockfd):
    window, entry, button = insert_view()
    button.configure(command=lambda:action("u", sockfd, entry, window))

def send_message(sockfd):
    window, entry, msg_entry, apply_button = send_message_view()
    apply_button.configure(command=lambda:action("s", sockfd, entry, window, msg_entry))


#Funkcja tworząca okno do wysyłania wiadomości
def send_message_view():
    sending_window = add_window()
    add_label(sending_window, "Wprowadź nazwę tematu: ")
    entry = add_entry(sending_window)
    add_label(sending_window, "Wprowadź wiadomość: ")
    msg_entry = add_entry(sending_window)
    apply_button = add_button(sending_window, "Zatwierdź")
    cancel_button = add_button(sending_window, "Anuluj")
    cancel_button.configure(command=sending_window.destroy)
    return sending_window, entry, msg_entry, apply_button

def main():
    
    #Łączenie się z serwerem, stworzenie okna aplikacji
    root, sockfd = connect(args.server_address, int(args.port))

    
    #Tworzenie i dodawanie przycisków do głównego okna
    b1, b2, b3, b4, b5, b6, b7 = buttons(root)

    #Przypisanie akcji do przycisków
    b1.configure(command=lambda:insert_topic(sockfd))
    b2.configure(command=lambda:send_message(sockfd))
    b3.configure(command=lambda:subscribe_topic(sockfd))
    b4.configure(command=lambda:unsubscribe_topic(sockfd))
    b5.configure(command=lambda:display_topics(sockfd))
    b6.configure(command=lambda:display_msg(sockfd))
    b7.configure(command=lambda:close_app(root, sockfd))

    if root != None:
        root.mainloop()

main()
    