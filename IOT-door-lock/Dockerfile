FROM php:8.1-apache

# Install mysqli extension (required to connect to MySQL)
RUN docker-php-ext-install mysqli

# Copy PHP application files to Apache web root
COPY src/php/ /var/www/html/

EXPOSE 80
